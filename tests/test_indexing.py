import os
import pytest
import h5py
import numpy as np
from mpi4py import MPI
import index_writer_py

NNODES = 10
SOURCE_OFFSET = 90
GROUP = "data"

@pytest.fixture(scope="module")
def mpi_comm():
    return MPI.COMM_WORLD

def generate_data(base, comm):
    rank = comm.Get_rank()
    if rank == 0:
        source_ids = np.repeat(np.arange(SOURCE_OFFSET, SOURCE_OFFSET + NNODES), NNODES)
        target_ids = np.tile(np.arange(NNODES), NNODES)

        with h5py.File(base, 'w') as file:
            g = file.create_group(GROUP)
            g.create_dataset("source_node_id", data=source_ids)
            g.create_dataset("target_node_id", data=target_ids)

    comm.Barrier()
    index_writer_py.write(base, SOURCE_OFFSET + NNODES, NNODES)
    comm.Barrier()

@pytest.fixture(scope="module")
def test_file(tmp_path_factory, mpi_comm):
    base = str(tmp_path_factory.mktemp("data") / "index_test.h5")
    generate_data(base, mpi_comm)
    return base

def test_indexing(test_file, mpi_comm):
    rank = mpi_comm.Get_rank()
    if rank == 0:
        with h5py.File(test_file, 'r') as f:
            g = f[GROUP]
            gidx = g['indices']
            sidx = gidx['source_to_target']
            tidx = gidx['target_to_source']

            source_ranges = sidx['node_id_to_ranges'][:]
            source_edges = sidx['range_to_edge_id'][:]
            target_ranges = tidx['node_id_to_ranges'][:]
            target_edges = tidx['range_to_edge_id'][:]

            # Check index sizes
            assert len(source_ranges) == SOURCE_OFFSET + NNODES
            assert len(target_ranges) == NNODES
            assert len(source_edges) == NNODES
            assert len(target_edges) == NNODES * NNODES

            # Check source index
            for i in range(SOURCE_OFFSET):
                assert source_ranges[i][0] == 0
                assert source_ranges[i][1] == 0

            for i in range(NNODES):
                assert source_ranges[SOURCE_OFFSET + i][0] == i
                assert source_ranges[SOURCE_OFFSET + i][1] == i + 1
                assert source_edges[i][0] == NNODES * i
                assert source_edges[i][1] == NNODES * (i + 1)

            # Check target index
            for i in range(NNODES):
                assert target_ranges[i][0] == NNODES * i
                assert target_ranges[i][1] == NNODES * (i + 1)

                for j in range(NNODES):
                    assert target_edges[NNODES * i + j][0] == NNODES * j + i
                    assert target_edges[NNODES * i + j][1] == NNODES * j + i + 1

    mpi_comm.Barrier()
