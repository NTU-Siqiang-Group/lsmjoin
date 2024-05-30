#!/bin/bash

make exp_runner
db_r_path="/home/weiping/tmp/db_r_breakdown"
db_s_path="/home/weiping/tmp/db_s_breakdown"
index_r_path="/home/weiping/tmp/index_r_breakdown"
index_s_path="/home/weiping/tmp/index_s_breakdown"

Million=1000000
# Dataset Size
s_tuples=$((10 * Million))
r_tuples=$((10 * Million))

output="test_7_entry_size.txt"
rm -f $output

list1=(128 32 8 4 2 1)
list2=(1 2 4)
# list=(1)s
for num2 in "${list2[@]}"; do
    for num1 in "${list1[@]}"; do
        ./exp_runner --J="INLJ" --r_index="Regular" --s_index="CLazy" --r_index_path=$index_r_path --s_index_path=$index_s_path --output_file=$output --db_r=$db_r_path --db_s=$db_s_path --s_tuples=$s_tuples --r_tuples=$r_tuples --B=$num1 --k_s=$num2

        ./exp_runner --J="SJ" --r_index="Regular" --s_index="CLazy" --r_index_path=$index_r_path --s_index_path=$index_s_path --output_file=$output --db_r=$db_r_path --db_s=$db_s_path --s_tuples=$s_tuples --r_tuples=$r_tuples --B=$num1 --k_s=$num2

        ./exp_runner --J="HJ" --r_index="Regular" --s_index="Regular" --r_index_path=$index_r_path --s_index_path=$index_s_path --output_file=$output --db_r=$db_r_path --db_s=$db_s_path --s_tuples=$s_tuples --r_tuples=$r_tuples --B=$num1 --k_s=$num2
    done
done