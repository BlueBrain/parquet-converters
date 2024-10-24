import index_writer_py
from mpi4py import MPI
import logging
import sys
import traceback

logger = logging.getLogger(__name__)

def write_index(filename, group_path, source_node_count, target_node_count):
    comm = MPI.COMM_WORLD
    rank = comm.Get_rank()
    size = comm.Get_size()
    logger.info(f"Rank {rank}/{size}: Starting write_index")
    try:
        logger.info(
            f"Rank {rank}/{size}: Filename: {filename}, Source node count: {source_node_count}, Target node count: {target_node_count}")

        start_time = MPI.Wtime()
        index_writer_py.write(filename, group_path, source_node_count, target_node_count)
        end_time = MPI.Wtime()

        logger.info(
            f"Rank {rank}/{size}: After index_writer_py.write, execution time: {end_time - start_time:.2f} seconds")

        # Ensure all processes have completed writing
        comm.Barrier()
        logger.info(f"Rank {rank}/{size}: After final barrier")
    except Exception as e:
        logger.error(f"Rank {rank}/{size}: Error writing index: {e}")
        logger.error(traceback.format_exc())
        raise
    finally:
        logger.info(f"Rank {rank}/{size}: Completed write_index")
        sys.stdout.flush()
