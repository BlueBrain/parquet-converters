> [!WARNING]
> The Blue Brain Project concluded in December 2024, so development has ceased under the BlueBrain GitHub organization.
> Future development will take place at: https://github.com/openbraininstitute/parquet-converters

# parquet-converters

Converts intermediate connectome building output files for consumption by different
programs:

* the binary output of [touchdetector][1] to Parquet, to be consumed by [functionalizer][2]
* the Parquet output of [functionalizer][2] to SONATA, to be read with [libsonata][3]

[1]: https://github.com/BlueBrain/touchdetector
[2]: https://github.com/BlueBrain/functionalizer
[3]: https://github.com/BlueBrain/libsonata

## Installation

Easiest with the package contained in the BlueBrain fork of [Spack](https://github.com/BlueBrain/spack).

Clone the project:

```console
$ gh repo clone BlueBrain/parquet-converters -- --recursive --shallow-submodules
```

The following dependencies should be provided:

* HDF5
* A MPI implementation, e.g., OpenMPI
* Apache Arrow and Parquet development packages, e.g., [following the official instructions][4]
* [`nlohmann_json`][5]
* [`range-v3`][6]
* [Catch2][7] (also as submodule)
* [CLI11][9] (also as submodule)
* [HighFive][8] (also as submodule)

After ensuring the availability of the dependencies, use the following to build the
utilities:
```console
$ cmake -B build -S . -GNinja -DCMAKE_CXX_COMPILER=$(which mpicxx)
$ cmake --build build
```

[4]: https://arrow.apache.org/install/
[5]: https://github.com/nlohmann/json
[6]: https://github.com/ericniebler/range-v3
[7]: https://github.com/catchorg/Catch2
[8]: https://github.com/BlueBrain/HighFive/
[9]: https://github.com/CLIUtils/CLI11

See also the [Dockerfile](Dockerfile) for a full build workflow.

## Usage

To convert TouchDetector output to Parquet (the output directory is the
current directory):
```
mpirun -np 4 touch2parquet $MY_TD_OUTPUT_DIRECTORY/touchesData.0
```
This will produce 4 Parquet files, adjust the parallelism accordingly to
create more files.

To produce a SONATA file with synapses contained in a population named
`All`:
```
mpirun -np 100 parquet2hdf5 $MY_FZ_OUTPUT_DIRECTORY/circuit.parquet edges.h5
All
```
Creating the synapse index requires a higher parallelism than the initial
conversion.

## Acknowledgment

The development of this software was supported by funding to the Blue Brain Project,
a research center of the École polytechnique fédérale de Lausanne (EPFL),
from the Swiss government's ETH Board of the Swiss Federal Institutes of Technology.

Copyright (c) 2017-2024 Blue Brain Project/EPFL
