# 删除构建目录下的所有文件
rm -rf ./build/*

# 重新运行 CMake
cd ./build
cmake ..

# 重新编译项目
make
