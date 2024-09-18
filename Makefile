path_name := 3_dtsled
cd_path := $(patsubst %,./%,$(path_name))

make_dev:
	cd $(cd_path) && make

clean:
	cd $(cd_path) && make clean

.PHONY : clean make_dev
	