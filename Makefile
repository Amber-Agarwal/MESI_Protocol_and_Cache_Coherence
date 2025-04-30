all:
	@clang++ -w cache.cpp >/dev/null
	@mv ./a.out ./L1simulate

clean:
	@rm ./L1simulate