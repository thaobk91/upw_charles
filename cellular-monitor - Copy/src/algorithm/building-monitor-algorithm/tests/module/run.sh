executable=./build/follower
# executable=./build/interrupt
# executable=./build/poll
# executable=./build/logger
# executable=./build/pollAndInterrupt

output_file_prefix="./output/temp_output_"

rm -r ./output/
mkdir ./output/
rm -r ./build/
mkdir ./build/

if [[ "$1" == "log" ]]; then
  echo "logging"
  make -k > ./build/build.log 2>&1 || (echo "build failed" && exit 0)
else
  make -k DISABLE_LOGGING=1 > ./build/build.log 2>&1 || (echo "build failed" && exit 0)
fi

index=0
declare -a outputs

run_in_parallel=1;

run_test () {
  this_index=$index
  # echo "i=$this_index $@"
  arguments="$@"
  index=$((index+1))
  run_executable() {
    echo "$this_index running $executable $arguments"
    temp_file="$output_file_prefix$this_index.txt"
    $executable $arguments > "$temp_file"
  }

  if [ $run_in_parallel -eq 1 ]; then
    run_executable &
  else
    run_executable
  fi
  # echo $output;
}


run_full_suite=1;
if [ $run_full_suite -eq 1 ]; then 
  echo "Happy Path"
  run_test 100.0 0.0 1000.0 1 0 -offsetBump=0
  run_test 100.0 0.02 1000.0 1 0 -offsetBump=0
  run_test 100.0 0.1 1000.0 1 0 -offsetBump=0
  run_test 100.0 1.0 1000.0 1 0 -offsetBump=0
  run_test 100.0 2.0 1000.0 1 0 -offsetBump=0
  run_test 100.0 4.0 1000.0 1 0 -offsetBump=0
  run_test 100.0 6.0 1000.0 1 0 -offsetBump=0
  run_test 100.0 2.0 1000.0 1 0 -offsetBump=0
  run_test 100.0 4.0 1000.0 1 0 -offsetBump=0
  run_test 100.0 6.0 1000.0 1 0 -offsetBump=0

  echo "Basic"
  run_test 100.0 0.0 1000.0 1 1
  run_test 100.0 0.02 1000.0 1 1
  run_test 100.0 0.1 1000.0 1 1
  run_test 100.0 1.0 1000.0 1 1
  run_test 100.0 2.0 1000.0 1 1
  run_test 100.0 4.0 1000.0 1 1
  run_test 100.0 6.0 1000.0 1 1
  run_test 100.0 13.0 1000.0 1 1
  run_test 100.0 20.0 1000.0 1 1
  run_test 100.0 22.0 1000.0 1 1
  run_test 100.0 25.0 1000.0 1 1
  run_test 100.0 28.0 1000.0 1 1
  run_test 100.0 30.0 1000.0 1 1

  echo "axis tests"
  run_test 100.0 2.0 1000.0 -xScale=1.0 -yScale=0.2 -zScale=0.1
  run_test 100.0 2.0 1000.0 -xScale=0.2 -yScale=0.1 -zScale=1.0
  run_test 100.0 2.0 1000.0 -xScale=0.1 -yScale=1.0 -zScale=0.2

  echo "pop up"
  run_test 100.0 0.0  1000.0 1 1 1
  run_test 100.0 0.02 1000.0 1 1 1 
  run_test 100.0 0.1 1000.0 1 1 1
  run_test 100.0 1.0 1000.0 1 1 1
  run_test 100.0 2.0 1000.0 1 1 1
  run_test 100.0 4.0 1000.0 1 1 1
  run_test 100.0 6.0 1000.0 1 1 1
  run_test 100.0 13.0 1000.0 1 1 1

  echo "worst case - high signal, high noise and fluctating and pulsing"
  run_test 1000.0 0.0 1000.0 1 1
  run_test 1000.0 0.02 1000.0 1 1
  run_test 1000.0 0.1 1000.0 1 1
  run_test 1000.0 1.0 1000.0 1 1
  run_test 1000.0 2.0 1000.0 1 1
  run_test 1000.0 4.0 1000.0 1 1
  run_test 1000.0 6.0 1000.0 1 1
  run_test 1000.0 13.0 1000.0 1 1

  echo "high signal, mid noise and fluctating and pulsing"
  run_test 600.0 0.0 1000.0 1 1
  run_test 600.0 0.02 1000.0 1 1
  run_test 600.0 0.1 1000.0 1 1
  run_test 600.0 1.0 1000.0 1 1
  run_test 600.0 2.0 1000.0 1 1
  run_test 600.0 4.0 1000.0 1 1
  run_test 600.0 6.0 1000.0 1 1
  run_test 600.0 13.0 1000.0 1 1

  echo "too high of noise"
  run_test 2000.0 0.00 1000.0 1 1
  # run_test 2000.0 0.02 1000.0 1 1
  # run_test 2000.0 0.1 1000.0 1 1
  run_test 2000.0 1.0 1000.0 1 1
  # run_test 2000.0 2.0 1000.0 1 1
  # run_test 2000.0 4.0 1000.0 1 1
  # run_test 2000.0 6.0 1000.0 1 1
  # run_test 2000.0 13.0 1000.0 1 1

  echo "edge case where z axis has really small inner cycle"
  run_test 30.0 8.0 300.0 0 0 -offsetBump=0 -xScale=0.5 -xInnerScale=0.2 -yInnerScale=0.9 -zInnerScale=0.20
fi

wait
echo "logging outputs:"
for i in $(seq 0 $((index - 1))); do
    # echo  #skip
  echo $(<$output_file_prefix$i.txt)
  # rm $output_file_prefix$i.txt  # Clean up
done