<div align="center">
  <img width="100" src="https://upload.wikimedia.org/wikipedia/commons/thumb/a/a5/Glazed-Donut.jpg/220px-Glazed-Donut.jpg">
  <h3>mpi_toroid</h3>
  <blockquote>Toy program implementing a torus-interconnect network topology using MPI.</blockquote>
</div>

## Build Instructions
```shell
$ git clone https://github.com/anpep/mpi_toroid
$ cd mpi_toroid
$ make
$ ./tools/generate_input.py 25 > input.dat
$ mpirun --oversubscribe -n 26 mpi_toroid 5 input.dat
```

## Open-source license
```
mpi_toroid -- Implements a torus-interconnect network topology using OpenMPI
Copyright (c) 2021 Ángel Pérez <angel@ttm.sh>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 2.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
```