import index_writer_py
from mpi4py import MPI
import logging
import sys
import traceback

def get_logger():
    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    ch = logging.StreamHandler(sys.stdout)
    ch.setFormatter(formatter)
    logger.addHandler(ch)
    return logger

logger = get_logger()

def init_mpi():
    logger.info("Initializing MPI")
    try:
        index_writer_py.init_mpi()
        logger.info("MPI initialized")
    except Exception as e:
        logger.error(f"Error initializing MPI: {e}")
        logger.error(traceback.format_exc())
        raise

def write_index(filename, source_node_count, target_node_count):
    comm = MPI.COMM_WORLD
    rank = comm.Get_rank()
    size = comm.Get_size()
    logger.info(f"Rank {rank}/{size}: Starting write_index")
    try:
        logger.info(f"Rank {rank}/{size}: Before barrier")
        comm.Barrier()
        logger.info(f"Rank {rank}/{size}: After barrier, before calling index_writer_py.write")
        logger.info(f"Rank {rank}/{size}: Filename: {filename}, Source node count: {source_node_count}, Target node count: {target_node_count}")
        
        start_time = MPI.Wtime()
        index_writer_py.write(filename, source_node_count, target_node_count)
        end_time = MPI.Wtime()
        
        logger.info(f"Rank {rank}/{size}: After index_writer_py.write, execution time: {end_time - start_time:.2f} seconds")
        
        # Ensure all processes have completed writing
        comm.Barrier()
        comm.Barrier()
        logger.info(f"Rank {rank}/{size}: After final barrier")
    except Exception as e:
        logger.error(f"Rank {rank}/{size}: Error writing index: {e}")
        logger.error(traceback.format_exc())
        raise
    finally:
        logger.info(f"Rank {rank}/{size}: Completed write_index")
        sys.stdout.flush()
