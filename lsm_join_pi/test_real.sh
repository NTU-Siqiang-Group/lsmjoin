#!/bin/bash
make exp_runner

db_r_path="/tmp/db_r_ywp"
db_s_path="/tmp/db_s_ywp"
data_path="/home/weiping/code/lsm_join_data/"
datasets=("user_id" "movie_id" "fb_200M_uint64" "osm_cellids_800M_uint64")
# datasets=("user_id")
num_loops=(1)

for dataset in "${datasets[@]}"; do
    if [ "$dataset" == "movie_id" ]; then
        public_r="${data_path}movie_info_movie_id"
        public_s="${data_path}cast_info_movie_id"
    elif [ "$dataset" == "user_id" ]; then
        public_r="${data_path}question_user_id"
        public_s="${data_path}so_user_user_id"
    else
        public_r="${data_path}${dataset}"
        public_s="${data_path}${dataset}"
    fi
    output="${dataset}.txt"
    rm -f $output
    for num_loop in "${num_loops[@]}"; do
        if [ "$num_loop" -eq 1 ]; then
            ingestion_flag=""
        else
            ingestion_flag="--ingestion"
        fi
        #index nested loop join
        echo "./exp_runner --M=64 --B=128 --ingestion --J="INLJ" --r_index="Regular" --s_index="Primary" --public_data --public_r=$public_r --public_s=$public_s --num_loop=$num_loop --output_file=$output --db_r=$db_r_path --db_s=$db_s_path"
        ./exp_runner --M=64 --B=128 --ingestion --J="INLJ" --r_index="Regular" --s_index="Primary" --public_data --public_r=$public_r --public_s=$public_s --num_loop=$num_loop --output_file=$output --db_r=$db_r_path --db_s=$db_s_path
        #sort mege join
        echo "./exp_runner --M=64 --B=128 $ingestion_flag --J="SJ" --r_index="Regular" --s_index="Primary" --public_data --public_r=$public_r --public_s=$public_s --num_loop=$num_loop --output_file=$output --db_r=$db_r_path --db_s=$db_s_path"
        ./exp_runner --M=64 --B=128 $ingestion_flag --J="SJ" --r_index="Regular" --s_index="Primary" --public_data --public_r=$public_r --public_s=$public_s --num_loop=$num_loop --output_file=$output --db_r=$db_r_path --db_s=$db_s_path
        ./exp_runner --M=64 --B=128 $ingestion_flag --J="SJ" --r_index="Eager" --s_index="Primary" --public_data --public_r=$public_r --public_s=$public_s --num_loop=$num_loop --output_file=$output --db_r=$db_r_path --db_s=$db_s_path
        ./exp_runner --M=64 --B=128 $ingestion_flag --J="SJ" --r_index="Lazy" --s_index="Primary" --public_data --public_r=$public_r --public_s=$public_s --num_loop=$num_loop --output_file=$output --db_r=$db_r_path --db_s=$db_s_path
        ./exp_runner --M=64 --B=128 $ingestion_flag --J="SJ" --r_index="Comp" --s_index="Primary" --public_data --public_r=$public_r --public_s=$public_s --num_loop=$num_loop --output_file=$output --db_r=$db_r_path --db_s=$db_s_path
        #covering sort mege join
        echo "./exp_runner --M=64 --B=128 --ingestion --J="SJ" --r_index="CEager" --s_index="Primary" --public_data --public_r=$public_r --public_s=$public_s --num_loop=$num_loop --output_file=$output --db_r=$db_r_path --db_s=$db_s_path"
        ./exp_runner --M=64 --B=128 --ingestion --J="SJ" --r_index="CEager" --s_index="Primary" --public_data --public_r=$public_r --public_s=$public_s --num_loop=$num_loop --output_file=$output --db_r=$db_r_path --db_s=$db_s_path
        ./exp_runner --M=64 --B=128 --ingestion --J="SJ" --r_index="CLazy" --s_index="Primary" --public_data --public_r=$public_r --public_s=$public_s --num_loop=$num_loop --output_file=$output --db_r=$db_r_path --db_s=$db_s_path
        ./exp_runner --M=64 --B=128 --ingestion --J="SJ" --r_index="CComp" --s_index="Primary" --public_data --public_r=$public_r --public_s=$public_s --num_loop=$num_loop --output_file=$output --db_r=$db_r_path --db_s=$db_s_path
        # hash join
        echo "./exp_runner --M=64 --B=128 --J="HJ" --r_index="Regular" --s_index="Primary" --public_data --public_r=$public_r --public_s=$public_s --num_loop=$num_loop --output_file=$output --db_r=$db_r_path --db_s=$db_s_path"
        ./exp_runner --M=64 --B=128 $ingestion_flag --J="HJ" --r_index="Regular" --s_index="Primary" --public_data --public_r=$public_r --public_s=$public_s --num_loop=$num_loop --output_file=$output --db_r=$db_r_path --db_s=$db_s_path
    done
done
