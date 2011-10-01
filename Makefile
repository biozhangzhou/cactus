# order is important, libraries first
modules = externalTools api pinchGraphs core setup blastAlignment baseAlignment normalisation matching reference phylogeny faces check pipeline progressive preprocessor

.PHONY: all %.all clean %.clean

all : ${modules:%=all.%}

all.%:
	cd $* && make all

clean:  ${modules:%=clean.%}
	rm -rf lib/*.h bin/*.dSYM

clean.%:
	cd $* && make clean
	
test: all
	python allTests.py