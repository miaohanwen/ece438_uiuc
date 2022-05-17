### Compiling and Running

To compile/run the main MP program, run `make` from the `mp4/` directory.  This will create an executable bash script to launch the python3 interpreter and run the MP code.  Alternatively, you may run the python script directly by executing `src/csma.py`.

### Gathering Simulation Data

To generate a new simulation dataset, run `make test && ./csma_test` or execute the python script directly by running `src/test.py`.  To configure the testing parameters, you will have to change the test script directly, as no command line interface has been given.  To modify the simulation, change lines 49 to 56 of `test.py` accordingly.

Simulation results are stored in a series of CSV files located in the `out/` directory, one file per test.  The data from the included CSV files has been combined in the Excel spreadsheet in the same folder.

The simulation may take up to 90 minutes to run, so be patient :)
