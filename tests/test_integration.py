import libsonata
import numpy as np
import numpy.testing as npt
import pandas as pd
import subprocess
import tempfile
from pathlib import Path


def generate_data(
    location: Path,
    source_nodes: int = 100,
    target_nodes: int = 100,
    avg_connections: int = 100,
    nfiles: int = 3,
) -> pd.DataFrame:
    """Generate SONATA test data and store it in `nfiles` files in `location`.

    Returns the generated data as a Pandas DataFrame.
    """
    if nfiles < 1:
        raise RuntimeError("need 1+ files to generate")

    sids = []
    tids = []

    rng = np.random.default_rng()
    target_pool = np.arange(target_nodes)

    for sid in range(source_nodes):
        (num,) = rng.integers(avg_connections, size=1)
        nums = np.sort(rng.choice(target_pool, num))
        tids.append(nums)
        sids.append(np.full_like(nums, sid))

    sids = np.concatenate(sids)
    tids = np.concatenate(tids)

    df = pd.DataFrame(
        {
            "source_node_id": sids,
            "target_node_id": tids,
            "edge_type_id": np.zeros_like(sids),
            "my_attribute": rng.standard_normal(len(sids)),
            "my_other_attribute": rng.integers(low=0, high=666, size=len(sids)),
        }
    )

    divisions = [0]
    divisions.extend(
        np.sort(
            rng.choice(
                np.arange(1, len(sids)), nfiles - 1, replace=False, shuffle=False
            )
        )
    )
    divisions.append(len(sids))

    for i in range(nfiles):
        slice = df.iloc[divisions[i] : divisions[i + 1]]
        slice.to_parquet(location / f"data{i}.parquet")

    return df


def test_conversion():
    with tempfile.TemporaryDirectory() as dirname:
        tmpdir = Path(dirname)

        parquet_name = tmpdir / "data.parquet"
        parquet_name.mkdir(parents=True, exist_ok=True)
        sonata_name = tmpdir / "data.h5"
        population_name = "d-fault"

        df = generate_data(parquet_name)

        subprocess.check_call(
            ["parquet2hdf5", parquet_name, sonata_name, population_name]
        )

        store = libsonata.EdgeStorage(sonata_name)
        assert store.population_names == {population_name}
        pop = store.open_population(population_name)
        assert len(pop) == len(df)

        npt.assert_array_equal(
            pop.get_attribute("my_attribute", pop.select_all()), df["my_attribute"]
        )
        npt.assert_array_equal(
            pop.get_attribute("my_other_attribute", pop.select_all()),
            df["my_other_attribute"],
        )


if __name__ == "__main__":
    test_conversion()
