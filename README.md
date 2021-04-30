<div align="center">
  <img width="100" src="https://pbs.twimg.com/media/EJKt_cAX0AU9TEF?format=jpg&name=900x900">
  <h3>mpi_x11blit</h3>
  <blockquote>
    Toy program that renders a raw bitmap in parallel using MPI.
  </blockquote>
  
  <img src="https://i.imgur.com/wRizemN.png">
</div>

## Build Instructions
### *NIX
```shell
$ git clone https://github.com/anpep/mpi_x11blit
$ cd mpi_x11blit
$ make
$ mpirun -n 1 mpi_x11blit 4 input.dat
```

### Windows
1. Download and install latest [MS-MPI runtime and SDK](https://github.com/microsoft/Microsoft-MPI/releases).
2. Clone this repository and open the `mpi_x11blit.sln` solution.
3. Build the project and run using `mpiexec` instead of `mpirun`:
```
mpiexec -n 1 x64\Release\mpi_x11blit.exe 4 input.dat
```

## Open-source license
```
mpi_x11blit -- Renders raw RGB data supplied by peers in parallel
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
