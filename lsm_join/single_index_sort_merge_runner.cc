// C++ program to implement
// external sorting using
// merge sort
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "index.hpp"
#include "merge.hpp"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/db.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/iostats_context.h"
#include "rocksdb/merge_operator.h"
#include "rocksdb/perf_context.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/statistics.h"
#include "rocksdb/table.h"
using namespace std;

class StringAppendOperator : public rocksdb::AssociativeMergeOperator {
 public:
  char delim_;  // The delimiter is inserted between elements

  // Constructor: also specify the delimiter character.
  StringAppendOperator(char delim_char) : delim_(delim_char) {}

  virtual bool Merge(const Slice& key, const Slice* existing_value,
                     const Slice& value, std::string* new_value,
                     Logger* logger) const override {
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

  virtual const char* Name() const override { return "StringAppendOperator"; }
};

// Driver code
int main(int argc, char* argv[]) {
  string index_type;
  char junk;
  uint64_t n;
  double m;
  uint64_t r_tuples = 1e7, s_tuples = 2e7;
  vector<uint64_t> R, S, P;
  double eps = 0.9;
  int k = 2;
  int c = 2;
  int M = 64;
  int B = 32;
  bool ingestion = false;

  for (int i = 1; i < argc; i++) {
    if (strncmp(argv[i], "--index=", 8) == 0) {
      index_type = argv[i] + 8;
    } else if (sscanf(argv[i], "--r_tuples=%lf%c", &m, &junk) == 1) {
      r_tuples = m;
    } else if (sscanf(argv[i], "--s_tuples=%lf%c", &m, &junk) == 1) {
      s_tuples = m;
    } else if (sscanf(argv[i], "--epsilon=%lf%c", &m, &junk) == 1) {
      eps = m;
    } else if (sscanf(argv[i], "--k=%lu%c", (unsigned long*)&n, &junk) == 1) {
      k = n;
    } else if (sscanf(argv[i], "--c=%lu%c", (unsigned long*)&n, &junk) == 1) {
      c = n;
    } else if (sscanf(argv[i], "--M=%lu%c", (unsigned long*)&n, &junk) == 1) {
      M = n;
    } else if (sscanf(argv[i], "--B=%lu%c", (unsigned long*)&n, &junk) == 1) {
      B = n;
    } else if (strcmp(argv[i], "--ingestion") == 0) {
      ingestion = true;
    }
  }

  cout << "index_type: " << index_type << endl;
  cout << "r_tuples: " << r_tuples << endl;
  cout << "s_tuples: " << s_tuples << endl;
  cout << "epsilon: " << eps << endl;
  cout << "k: " << k << endl;
  cout << "B: " << B << endl;
  cout << "M: " << M << "MB" << endl;
  M = n << 20;

  struct timespec t1, t2, t3, t4;
  // vector<uint64_t> data;
  // ifstream in("/home/weiping/code/SOSD/data/wiki_ts_200M_uint64",
  // ios::binary); uint64_t tuples; in.read(reinterpret_cast<char*>(&tuples),
  // sizeof(string)); data.resize(tuples);
  // in.read(reinterpret_cast<char*>(data.data()), tuples * sizeof(uint64_t));
  // in.close();
  auto rng = std::default_random_engine{};
  // tuples = tuples / 10;
  // tuples = 100;
  int PRIMARY_SIZE = 10;
  int SECONDARY_SIZE = 10;
  int VALUE_SIZE = 4096 / B - SECONDARY_SIZE;
  string db_r_path = "/tmp/wiki_128_R";
  string db_s_path = "/tmp/wiki_128_S";
  rocksdb::Options rocksdb_opt;
  rocksdb_opt.create_if_missing = true;
  rocksdb_opt.compression = kNoCompression;
  rocksdb_opt.bottommost_compression = kNoCompression;
  rocksdb::BlockBasedTableOptions table_options;
  table_options.filter_policy.reset(NewBloomFilterPolicy(10, false));
  table_options.no_block_cache = true;
  rocksdb_opt.table_factory.reset(NewBlockBasedTableFactory(table_options));
  rocksdb_opt.statistics = rocksdb::CreateDBStatistics();
  if (ingestion) {
    rocksdb::DestroyDB(db_r_path, rocksdb::Options());
    rocksdb::DestroyDB(db_s_path, rocksdb::Options());
  }
  rocksdb::DB* db_r = nullptr;
  rocksdb::DB::Open(rocksdb_opt, db_r_path, &db_r);
  rocksdb::DB* db_s = nullptr;
  rocksdb::DB::Open(rocksdb_opt, db_s_path, &db_s);
  rocksdb::DB* index_s = nullptr;

  uint64_t random_key;
  string tmp_key, tmp_value;
  string tmp;
  std::map<std::string, uint64_t> stats;
  generatePK(s_tuples, P, c);
  generateData(r_tuples, s_tuples, eps, k, R, S);
  shuffle(R.begin(), R.end(), rng);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  if (ingestion) {
    cout << "ingesting r " << r_tuples << " tuples with size "
         << PRIMARY_SIZE + VALUE_SIZE << "... " << endl;
    ingest_data(r_tuples, db_r, vector<uint64_t>(), R, VALUE_SIZE,
                SECONDARY_SIZE, PRIMARY_SIZE);
  }
  clock_gettime(CLOCK_MONOTONIC, &t2);
  auto ingest_time1 =
      (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) / 1000000000.0;

  string index_path = "/tmp/wiki_128_R_index";
  rocksdb::DestroyDB(index_path, rocksdb::Options());
  if (index_type == "comp") {
    table_options.filter_policy.reset(NewBloomFilterPolicy(10));
    table_options.whole_key_filtering = false;
    rocksdb_opt.table_factory.reset(NewBlockBasedTableFactory(table_options));
    rocksdb_opt.prefix_extractor.reset(
        NewCappedPrefixTransform(SECONDARY_SIZE));
  }
  if (index_type == "lazy") {
    rocksdb_opt.merge_operator.reset(new StringAppendOperator(':'));
  }
  rocksdb_opt.write_buffer_size = (M - 3 * 4096) / 2;
  rocksdb_opt.max_bytes_for_level_base =
      rocksdb_opt.write_buffer_size *
      rocksdb_opt.max_bytes_for_level_multiplier;
  clock_gettime(CLOCK_MONOTONIC, &t1);
  clock_gettime(CLOCK_MONOTONIC, &t2);
  auto index_build_time1 =
      (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) / 1000000000.0;
  // cout << "lazy_index_build_time: " << index_build_time1 << endl;

  shuffle(S.begin(), S.end(), rng);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  if (ingestion) {
    cout << "ingesting s " << s_tuples << " tuples with size "
         << PRIMARY_SIZE + VALUE_SIZE << "... " << endl;
    ingest_data(s_tuples, db_s, P, S, VALUE_SIZE, SECONDARY_SIZE, PRIMARY_SIZE);
  }
  clock_gettime(CLOCK_MONOTONIC, &t2);
  auto ingest_time2 =
      (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) / 1000000000.0;

  index_path = "/tmp/wiki_128_S_index";
  rocksdb::DestroyDB(index_path, Options());
  rocksdb::DB::Open(rocksdb_opt, index_path, &index_s);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  if (index_type == "lazy")
    build_lazy_index(db_s, index_s, S.data(), P, s_tuples, VALUE_SIZE,
                     SECONDARY_SIZE, PRIMARY_SIZE);
  else if (index_type == "eager")
    build_eager_index(db_s, index_s, S.data(), P, s_tuples, VALUE_SIZE,
                      SECONDARY_SIZE, PRIMARY_SIZE);
  else
    build_composite_index(db_s, index_s, S.data(), P, s_tuples, VALUE_SIZE,
                          SECONDARY_SIZE, PRIMARY_SIZE);
  clock_gettime(CLOCK_MONOTONIC, &t2);
  auto index_build_time2 =
      (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) / 1000000000.0;

  cout << index_type << endl;
  cout << "ingest_time: " << ingest_time1 + ingest_time1 << " (" << ingest_time1
       << "+" << ingest_time2 << ")" << endl;
  cout << "index_build_time: " << index_build_time1 + index_build_time2 << " ("
       << index_build_time1 << "+" << index_build_time2 << ")" << endl;
  // vector<uint64_t>().swap(S);

  cout << "Merging" << endl;
  clock_gettime(CLOCK_MONOTONIC, &t1);
  // double run_size = 10;

  rocksdb::Iterator* it_s = index_s->NewIterator(ReadOptions());
  int matches = 0, count1 = 1, count2 = 0;
  string temp_r_key, temp_r_value, temp_s_key, temp_s_value, value_r, value_s;
  std::vector<std::string> value_split;
  Status s;
  rocksdb::get_perf_context()->Reset();
  int run_size = int(M / (PRIMARY_SIZE + VALUE_SIZE)) - 1;
  int num_ways = r_tuples / run_size + 1;
  string output_file_r = "/tmp/output_r.txt";
  externalSort(db_r, output_file_r, num_ways, run_size, VALUE_SIZE,
               SECONDARY_SIZE);
  ifstream in_r(output_file_r);
  string line_r;
  char* buf1 = new char[4096];
  in_r.rdbuf()->pubsetbuf(buf1, 4096);
  if (!getline(in_r, line_r)) {
    cout << "error" << endl;
  };
  it_s->SeekToFirst();
  double val_time = 0.0, get_time = 0.0;
  if (index_type == "lazy" || index_type == "eager") {
    while (line_r != "" && it_s->Valid()) {
      std::istringstream iss_r(line_r);
      getline(iss_r, temp_r_key, ',');
      getline(iss_r, temp_r_value);
      temp_s_key = it_s->key().ToString();
      temp_s_value = it_s->value().ToString();
      if (temp_r_key == temp_s_key) {
        clock_gettime(CLOCK_MONOTONIC, &t3);
        while (getline(in_r, line_r)) {
          std::istringstream temp_iss_r(line_r);
          std::string temp_first_r, temp_second_r;
          if (getline(temp_iss_r, temp_first_r, ',') &&
              getline(temp_iss_r, temp_second_r)) {
            if (temp_first_r == temp_r_key) {
              // cout << "first_r: " << temp_first_r << endl;
              count1++;
              continue;
            } else {
              break;
            }
          }
        }
        value_split =
            boost::split(value_split, temp_s_value, boost::is_any_of(":"));
        for (auto x : value_split) {
          s = db_s->Get(ReadOptions(), x.substr(0, PRIMARY_SIZE), &tmp);
          if (s.ok() && tmp.substr(0, SECONDARY_SIZE) == temp_s_key) count2++;
        }
        matches += count1 * count2;
        clock_gettime(CLOCK_MONOTONIC, &t4);
        val_time +=
            (t4.tv_sec - t3.tv_sec) + (t4.tv_nsec - t3.tv_nsec) / 1000000000.0;
        count1 = 1;
        count2 = 0;
        it_s->Next();
      } else if (temp_r_key < temp_s_key) {
        if (!getline(in_r, line_r)) break;
      } else if (temp_r_key > temp_s_key)
        it_s->Next();
    }
  } else {
    count1 = 0;
    cout << "composite" << endl;
    while (line_r != "" && it_s->Valid()) {
      std::istringstream iss_r(line_r);
      getline(iss_r, temp_r_key, ',');
      getline(iss_r, temp_r_value);
      temp_s_key = it_s->key().ToString().substr(0, SECONDARY_SIZE);
      temp_s_value =
          it_s->key().ToString().substr(SECONDARY_SIZE, PRIMARY_SIZE);
      if (temp_r_key == temp_s_key) {
        clock_gettime(CLOCK_MONOTONIC, &t3);
        count1++;
        s = db_s->Get(ReadOptions(), temp_s_value, &value_s);
        if (s.ok() && value_s.substr(0, SECONDARY_SIZE) == temp_s_key) count2++;
        tmp = temp_r_key;
        while (getline(in_r, line_r)) {
          std::istringstream temp_iss_r(line_r);
          std::string temp_first_r, temp_second_r;
          if (getline(temp_iss_r, temp_first_r, ',') &&
              getline(temp_iss_r, temp_second_r)) {
            if (temp_first_r == temp_r_key) {
              // cout << "first_r: " << temp_first_r << endl;
              count1++;
              continue;
            } else {
              break;
            }
          }
        }
        while (it_s->Valid()) {
          it_s->Next();
          if (!it_s->Valid()) break;
          temp_s_key = it_s->key().ToString().substr(0, SECONDARY_SIZE);
          temp_s_value =
              it_s->key().ToString().substr(SECONDARY_SIZE, PRIMARY_SIZE);
          if (temp_s_key == tmp) {
            s = db_s->Get(ReadOptions(), temp_s_value, &value_s);
            if (s.ok() && value_s.substr(0, SECONDARY_SIZE) == temp_s_key)
              count2++;
          } else
            break;
        }
        matches += count1 * count2;
        clock_gettime(CLOCK_MONOTONIC, &t4);
        val_time +=
            (t4.tv_sec - t3.tv_sec) + (t4.tv_nsec - t3.tv_nsec) / 1000000000.0;
        count1 = 0;
        count2 = 0;
      } else if (temp_r_key < temp_s_key) {
        if (!getline(in_r, line_r)) break;
      } else if (temp_r_key > temp_s_key)
        it_s->Next();
    }
  }

  cout << "join read io: " << get_perf_context()->block_read_count << endl;
  cout << "matches: " << matches << endl;
  cout << "val_time: " << val_time << endl;
  cout << "get_time: " << get_time << endl;
  clock_gettime(CLOCK_MONOTONIC, &t2);
  auto sort_merge_time =
      (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) / 1000000000.0;
  cout << "sort_merge_time: " << sort_merge_time << endl;
  delete it_s;
  db_r->Close();
  db_s->Close();
  index_s->Close();
  delete db_r;
  delete db_s;
  delete index_s;
}
