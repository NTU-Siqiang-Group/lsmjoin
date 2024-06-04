import pandas as pd
import csv

lookup_dict = {
    ("Regular", "Primary", "INLJ"): "P-INLJ",
    ("Regular", "Primary", "SJ"): "P-ISJ",
    ("Eager", "Primary", "SJ"): "P-Eager-ISJ",
    ("Lazy", "Primary", "SJ"): "P-Lazy-ISJ",
    ("Comp", "Primary", "SJ"): "P-Comp-ISJ",
    ("CEager", "Primary", "SJ"): "P-CEager-ISJ",
    ("CLazy", "Primary", "SJ"): "P-CLazy-ISJ",
    ("CComp", "Primary", "SJ"): "P-CComp-ISJ",
    ("Regular", "Primary", "HJ"): "P-Grace-HJ",
    ("Regular", "Eager", "INLJ"): "Eager-INLJ",
    ("Regular", "Lazy", "INLJ"): "Lazy-INLJ",
    ("Regular", "Comp", "INLJ"): "Comp-INLJ",
    ("Regular", "CEager", "INLJ"): "CEager-INLJ",
    ("Regular", "CLazy", "INLJ"): "CLazy-INLJ",
    ("Regular", "CComp", "INLJ"): "CComp-INLJ",
    ("Regular", "Regular", "SJ"): "NISJ",
    ("Regular", "Eager", "SJ"): "1Eager-ISJ",
    ("Regular", "Lazy", "SJ"): "1Lazy-ISJ",
    ("Regular", "Comp", "SJ"): "1Comp-ISJ",
    ("Eager", "Eager", "SJ"): "2Eager-ISJ",
    ("Comp", "Comp", "SJ"): "2Comp-ISJ",
    ("Lazy", "Lazy", "SJ"): "2Lazy-ISJ",
    ("Regular", "CEager", "SJ"): "1CEager-ISJ",
    ("Regular", "CLazy", "SJ"): "1CLazy-ISJ",
    ("Regular", "CComp", "SJ"): "1CComp-ISJ",
    ("CEager", "CEager", "SJ"): "2CEager-ISJ",
    ("CLazy", "CLazy", "SJ"): "2CLazy-ISJ",
    ("CComp", "CComp", "SJ"): "2CComp-ISJ",
    ("Regular", "Regular", "HJ"): "Grace-HJ",
}

test_names = [
    # "bpk",
    # "buffer_size_t",
    # "buffer_size",
    # "cache_size",
    # "skewness",
    # "B",
    # "num_loop",
    "K",
    # "dataset_size",
    # "dataratio",
    # "c",
    # "k",
    # "buffer_size",
    "T",
    "T_t",
]


def write_overall_csv():
    # datasets=("user_id" "movie_id" "fb_200M_uint64" "osm_cellids_800M_uint64" "unif" "skew")
    datasets = [
        "user_id",
        "movie_id",
        "fb_200M_uint64",
        "osm_cellids_800M_uint64",
        "unif",
        "skew",
    ]
    names = ["User", "Movie", "Face", "OSM", "Unif", "Skew"]
    headers = []
    all_rows = []
    for i, dataset in enumerate(datasets):
        surfixs = ["", "_5nlj", "_5sj"]
        for surfix in surfixs:
            with open(f"lsm_join/{dataset+surfix}.txt", "r") as file:
                data = file.read()
                lines = data.strip().split("\n")
                rows = []
                for line in lines:
                    if line == "-------------------------":
                        continue
                    # Split each line into key-value pairs
                    pairs = line.split()
                    row = {}
                    for pair in pairs:
                        if "=" in pair:
                            key, value = pair.split("=", 1)
                            row[key] = value
                    row["dataset"] = names[i]
                    rows.append(row)

                if i == 0:
                    # Get the headers from the keys of the first row
                    headers = rows[0].keys()
                all_rows.extend(rows)

    # Write to CSV
    with open(f"lsm_join/csv_result/overall.csv", "w", newline="") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=headers)
        writer.writeheader()
        for row in all_rows:
            writer.writerow(row)


def write_csv_from_txt(test_name):
    path = "lsm_join"
    # iterate through each test and write to a csv file
    with open(f"{path}/test_{test_name}.txt", "r") as file:
        # with open(f"{path}/{test_name}.txt", "r") as file:
        data = file.read()
        lines = data.strip().split("\n")
        rows = []
        for line in lines:
            if line == "-------------------------":
                continue
            # Split each line into key-value pairs
            pairs = line.split()
            row = {}
            for pair in pairs:
                if "=" in pair:
                    key, value = pair.split("=", 1)
                    row[key] = value
            rows.append(row)

        # Get the headers from the keys of the first row
        headers = rows[0].keys()

        # Write to CSV
        with open(f"lsm_join/csv_result/{test_name}.csv", "w", newline="") as csvfile:
            writer = csv.DictWriter(csvfile, fieldnames=headers)
            writer.writeheader()
            for row in rows:
                writer.writerow(row)


def process_csv(test_name):
    df = pd.read_csv(f"lsm_join/csv_result/{test_name}.csv")

    df["label"] = df.apply(
        lambda x: lookup_dict[(x["r_index"], x["s_index"], x["join_algorithm"])], axis=1
    )

    column_save = [
        "sum_join_time",
        "sum_index_build_time",
        "label",
    ]

    if test_name == "T_t":
        column_save.append("T")
        df["theory"] = 1
    elif test_name == "T":
        column_save.append("T")
        df["theory"] = 0
    elif test_name == "K":
        column_save.append("K")
    elif test_name == "buffer_size":
        column_save.append("M")
        column_save.append("cache_hit_rate")
        df["theory"] = 0
        column_save.append("theory")
    elif test_name == "buffer_size_t":
        column_save.append("M")
        column_save.append("cache_hit_rate")
        df["theory"] = 1
        column_save.append("theory")
    elif test_name == "cache_size":
        column_save.append("cache_size")
        column_save.append("cache_hit_rate")
    elif test_name == "bpk":
        column_save.append("bpk")
        column_save.append("false_positive_rate")
        column_save.append("cache_hit_rate")
    elif test_name == "overall":
        column_save.append("dataset")
    elif test_name == "c":
        column_save.append("c_r")
    elif test_name == "c_k":
        column_save.append("c_r")
        column_save.append("k_r")
        column_save.append("k_s")
    elif test_name == "c_skewness":
        column_save.append("c_r")
        column_save.append("k_r")
        column_save.append("k_s")
    elif test_name == "loops_size":
        column_save.append("num_loop")
        column_save.append("s_tuples")
        column_save.append("r_tuples")
    elif test_name == "entry_size":
        column_save.append("B")
        column_save.append("k_s")
        column_save.append("bpk")
    elif "workload" in test_name:
        column_save.append("num_loop")
    elif test_name == "dataratio":
        df["dataratio"] = df["r_tuples"] / df["s_tuples"]
        # 如果是整数，不保留；如果是小数，保留1位
        df["dataratio"] = df["dataratio"].apply(
            lambda x: int(x) if x == int(x) else round(x, 1)
        )
        column_save.append("dataratio")
        column_save.append("r_tuples")
        column_save.append("s_tuples")
    elif test_name == "dataset_size":
        column_save.append("r_tuples")
        column_save.append("s_tuples")
    elif test_name == "k":
        column_save.append("k_r")
    elif "num_loop" in test_name:
        column_save.append("num_loop")
        column_save.append("join_time_list")
        column_save.append("index_build_time_list")
    elif test_name == "skewness":
        column_save.append("k_s")
    elif test_name == "covering":
        column_save.append("num_loop")
        column_save.append("noncovering")
        column_save.append("B")

    # Save to csv
    df[column_save].to_csv(f"lsm_join/csv_result/{test_name}.csv", index=False)
