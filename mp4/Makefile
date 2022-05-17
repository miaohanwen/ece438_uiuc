.PHONY: all clean test

all : csma

# The @ in front of the following commands suppresses
# the usual echo to provide a clean make output.
csma: src/csma.py
	@echo '#!/bin/bash' > csma
	@echo 'python3 src/csma.py $$1' >> csma
	@chmod +x csma

test: src/test.py
	@echo '#!/bin/bash' > csma_test
	@echo 'python3 src/test.py' >> csma_test
	@chmod +x csma_test

clean :
	$(RM) obj/*.o csma csma_test
