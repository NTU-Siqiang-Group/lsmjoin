#pragma once

#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "exp_utils.hpp"
#include "expconfig.hpp"
#include "index.hpp"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/db.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/iostats_context.h"
#include "rocksdb/merge_operator.h"
#include "rocksdb/perf_context.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/statistics.h"
#include "rocksdb/table.h"

using namespace ROCKSDB_NAMESPACE;

class StringAppendOperator : public rocksdb::AssociativeMergeOperator {
 public:
  char delim_;  // The delimiter is inserted between elements

  // Constructor: also specify the delimiter character.
  StringAppendOperator(char delim_char) : delim_(delim_char) {}

  virtual bool Merge(const Slice &key, const Slice *existing_value,
                     const Slice &value, std::string *new_value,
                     Logger *logger) const override {
    // Clear the *new_value for writing.
    assert(new_value);
    new_value->clear();

    if (!existing_value) {
      // No existing_value. Set *new_value = value
      new_value->assign(value.data(), value.size());
    } else {
      // Generic append (existing_value != null).
      // Reserve *new_value to correct size, and apply concatenation.
      new_value->reserve(existing_value->size() + 1 + value.size());
      new_value->assign(existing_value->data(), existing_value->size());
      new_value->append(1, delim_);
      new_value->append(value.data(), value.size());
    }
    return true;
  }

  virtual const char *Name() const override { return "StringAppendOperator"; }
};

class ExpContext {
 public:
  static ExpContext &getInstance() {
    static ExpContext instance;
    return instance;
  }

  vector<uint64_t> ReadDatabase(string &file_path) {
    ifstream in(file_path, ios::binary);
    if (!in) {
      std::cerr << "Cannot open file.\n";
      return {};
    }
    uint64_t tuples;
    in.read(reinterpret_cast<char *>(&tuples), sizeof(uint64_t));
    tuples = min(static_cast<uint64_t>(10000000ULL), tuples);

    uint64_t part_size = tuples / config.num_loop;
    uint64_t last_part_size = tuples - (part_size * (config.num_loop - 1));

    std::vector<uint64_t> data;
    data.resize(part_size);
    if (config.this_loop == config.num_loop - 1) {
      data.resize(last_part_size);
    }
    in.seekg(sizeof(uint64_t) * part_size * config.this_loop + sizeof(uint64_t),
             ios::beg);
    in.read(reinterpret_cast<char *>(data.data()),
            sizeof(uint64_t) * data.size());
    int modulus = static_cast<int>(std::pow(10, config.PRIMARY_SIZE));
    for (uint64_t &num : data) {
      num %= modulus;
    }
    cout << "Read part " << config.this_loop + 1 << " of " << config.num_loop
         << " with " << data.size() << " tuples" << endl;
    in.close();
    return data;
  }

  void InitDB() {
    rocksdb_opt.create_if_missing = true;
    rocksdb_opt.compression = kNoCompression;
    rocksdb_opt.bottommost_compression = kNoCompression;
    rocksdb_opt.statistics = rocksdb::CreateDBStatistics();
    table_options.filter_policy.reset(NewBloomFilterPolicy(10));
    table_options.no_block_cache = true;
    rocksdb_opt.table_factory.reset(NewBlockBasedTableFactory(table_options));
    if (config.ingestion) {
      rocksdb::DestroyDB(config.db_r, rocksdb::Options());
      rocksdb::DestroyDB(config.db_s, rocksdb::Options());
    }
    db_r = nullptr;
    db_s = nullptr;
    index_r = nullptr;
    rocksdb::DB::Open(rocksdb_opt, config.db_r, &db_r);
    rocksdb::DB::Open(rocksdb_opt, config.db_s, &db_s);

    if (config.r_index == "Comp" || config.r_index == "CComp") {
      table_options.filter_policy.reset(NewBloomFilterPolicy(10));
      table_options.whole_key_filtering = false;
      rocksdb_opt.table_factory.reset(NewBlockBasedTableFactory(table_options));
      rocksdb_opt.prefix_extractor.reset(
          NewCappedPrefixTransform(config.SECONDARY_SIZE));
    }
    if (config.r_index == "Lazy" || config.r_index == "CLazy") {
      rocksdb_opt.merge_operator.reset(new StringAppendOperator(':'));
    }
  }

  void GenerateData(vector<uint64_t> &R, vector<uint64_t> &S,
                    vector<uint64_t> &P) {
    if (config.is_public_data) {
      R = ReadDatabase(config.public_r);
      config.r_tuples = R.size();
      S = ReadDatabase(config.public_s);
      config.s_tuples = S.size();
    } else {
      generateData(config.s_tuples, config.r_tuples, config.eps, config.k, S,
                   R);
    }
    generatePK(config.r_tuples, P, config.c);  // generate Primary keys for R
  }

  auto regularIngestS(vector<uint64_t> &S) {
    shuffle(S.begin(), S.end(), rng);
    // ingestion phrase
    Timer timer1 = Timer();

    if (config.ingestion) {
      cout << "ingesting s " << config.s_tuples << " tuples with size "
           << config.PRIMARY_SIZE + config.VALUE_SIZE << "... " << endl;
      ingest_pk_data(config.s_tuples, db_s, S, config.VALUE_SIZE,
                     config.SECONDARY_SIZE, config.PRIMARY_SIZE);
    }
    auto ingest_time1 = timer1.elapsed();
    return ingest_time1;
  }

  auto regularIngestR(vector<uint64_t> &R, vector<uint64_t> &P) {
    shuffle(R.begin(), R.end(), rng);
    Timer timer1 = Timer();
    if (config.ingestion) {
      cout << "ingesting r " << config.r_tuples << " tuples with size "
           << config.PRIMARY_SIZE + config.VALUE_SIZE << "... " << endl;
      ingest_data(config.r_tuples, db_r, P, R, config.VALUE_SIZE,
                  config.SECONDARY_SIZE, config.PRIMARY_SIZE);
    }
    auto ingest_time2 = timer1.elapsed();
    return ingest_time2;
  }

  void Ingest(vector<uint64_t> &R, vector<uint64_t> &S, vector<uint64_t> &P,
              bool isRegular_R = true, bool isRegular_S = true) {
    double ingest_time1 = 0.0, ingest_time2 = 0.0;
    if (isRegular_S) {
      ingest_time1 = regularIngestS(S);
    }
    if (isRegular_R) {
      ingest_time2 = regularIngestR(R, P);
    }
    // cout << "ingest_time: " << ingest_time1 + ingest_time2 << " ("
    //      << ingest_time1 << "+" << ingest_time2 << ")" << endl;
  }

  auto BuildNonCoveringIndex(vector<uint64_t> &R, vector<uint64_t> &P) {
    Timer timer1 = Timer();
    if (config.r_index == "Lazy")
      build_lazy_index(db_r, index_r, R.data(), P, config.r_tuples,
                       config.VALUE_SIZE, config.SECONDARY_SIZE,
                       config.PRIMARY_SIZE);
    else if (config.r_index == "Eager")
      build_eager_index(db_r, index_r, R.data(), P, config.r_tuples,
                        config.VALUE_SIZE, config.SECONDARY_SIZE,
                        config.PRIMARY_SIZE);
    else
      build_composite_index(db_r, index_r, R.data(), P, config.r_tuples,
                            config.VALUE_SIZE, config.SECONDARY_SIZE,
                            config.PRIMARY_SIZE);
    auto index_build_time2 = timer1.elapsed();

    cout << config.r_index << endl;
    return index_build_time2;
  }

  auto BuildCoveringIndex(vector<uint64_t> &R, vector<uint64_t> &P) {
    shuffle(R.begin(), R.end(), rng);

    int PRIMARY_SIZE = config.PRIMARY_SIZE,
        SECONDARY_SIZE = config.SECONDARY_SIZE, VALUE_SIZE = config.VALUE_SIZE;
    string index_type = config.r_index;
    int r_tuples = config.r_tuples;

    cout << "ingesting and building covering index r " << r_tuples
         << " tuples with size " << PRIMARY_SIZE + VALUE_SIZE << "... " << endl;
    double ingest_time2 = 0.0;
    if (index_type == "CComp")
      ingest_time2 += build_covering_composite_index(
          db_r, index_r, R.data(), P, r_tuples, VALUE_SIZE, SECONDARY_SIZE,
          PRIMARY_SIZE);
    else if (index_type == "CLazy")
      ingest_time2 +=
          build_covering_lazy_index(db_r, index_r, R.data(), P, r_tuples,
                                    VALUE_SIZE, SECONDARY_SIZE, PRIMARY_SIZE);
    else
      ingest_time2 +=
          build_covering_eager_index(db_r, index_r, R.data(), P, r_tuples,
                                     VALUE_SIZE, SECONDARY_SIZE, PRIMARY_SIZE);

    cout << index_type << endl;
    return ingest_time2;
  }

  // build index for R
  double BuildIndex(vector<uint64_t> &R, vector<uint64_t> &P,
                    bool is_covering = false) {
    if (config.this_loop == 0) {
      rocksdb_opt.write_buffer_size = (config.M - 3 * 4096) / 2;
      rocksdb_opt.max_bytes_for_level_base =
          rocksdb_opt.write_buffer_size *
          rocksdb_opt.max_bytes_for_level_multiplier;
      // build index
      rocksdb::DestroyDB(config.r_index_path, Options());
      rocksdb::DB::Open(rocksdb_opt, config.r_index_path, &index_r);
    }

    // cout << "index_build_time: " << index_build_time1 + index_build_time2
    //      << " (" << index_build_time1 << "+" << index_build_time2 << ")"
    //      << endl;
    // TODO: index_build_time1 is not used
    double index_build_time2 = 0.0;
    if (is_covering) {
      index_build_time2 = BuildCoveringIndex(R, P);
    } else {
      index_build_time2 = BuildNonCoveringIndex(R, P);
    }

    cout << "index_build_time: " << index_build_time2 << endl;
    return index_build_time2;
  }

  // forbid copy and move
  ExpContext(ExpContext const &) = delete;
  void operator=(ExpContext const &) = delete;

  // // config parameters
  // vector<uint64_t> R, S,
  //     P;  // R: left relation, S: right relation, P: primary keys
  rocksdb::DB *db_r;
  rocksdb::DB *db_s;
  rocksdb::Options rocksdb_opt;
  rocksdb::BlockBasedTableOptions table_options;
  std::default_random_engine rng;
  ExpConfig &config;
  rocksdb::DB *index_r;

 private:
  ExpContext()
      : config(ExpConfig::getInstance()), rng(std::default_random_engine{}) {}
};