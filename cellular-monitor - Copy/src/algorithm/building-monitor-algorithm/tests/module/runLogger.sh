
# executable=./build/interrupt
# executable=./build/poll
# executable=./build/logger
executable=./build/logger

rm -r ./build/
mkdir ./build/

make -k > ./build/build.log 2>&1 || (echo "build failed" && exit 0)

echo "logger"
$executable 0.0 6.0 1000.0 0 0 -xInnerScale=0.5 -yInnerScale=0.5 -zInnerScale=0.3 > ./output/logger.log 2>&1

