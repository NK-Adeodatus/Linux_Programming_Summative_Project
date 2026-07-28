"""
test_sensor_analysis.py

Test program for the sensor_analysis C extension module.

Simulates a batch of environmental sensor readings (e.g. soil moisture
percentages) and exercises every function exposed by the module:
    average(data)
    range_value(data)
    variance(data)
    count_above(data, limit)
    statistics(data)

It also verifies that invalid input (wrong type, empty dataset) and a
boundary condition (single-element dataset for variance) correctly raise
Python exceptions instead of crashing.
"""

import sensor_analysis


def print_header(title):
    print("\n" + "=" * 60)
    print(title)
    print("=" * 60)


def main():
    # Sample sensor data: simulated soil moisture % readings from an
    # IoT device over a short window of time.
    readings = [23.5, 25.1, 19.8, 30.2, 22.0, 27.6, 21.4, 24.9]
    print_header("Sample sensor data")
    print(f"readings = {readings}")

    # --- average() ---------------------------------------------------
    print_header("average(data)")
    avg = sensor_analysis.average(readings)
    print(f"average = {avg:.4f}")
    assert abs(avg - (sum(readings) / len(readings))) < 1e-9

    # --- range_value() -------------------------------------------------
    print_header("range_value(data)")
    rng = sensor_analysis.range_value(readings)
    print(f"range_value = {rng:.4f}")
    assert abs(rng - (max(readings) - min(readings))) < 1e-9

    # --- variance() ----------------------------------------------------
    print_header("variance(data)")
    var = sensor_analysis.variance(readings)
    print(f"variance (sample) = {var:.4f}")
    # Cross-check against Python's own two-pass calculation.
    mean = sum(readings) / len(readings)
    expected_var = sum((x - mean) ** 2 for x in readings) / (len(readings) - 1)
    assert abs(var - expected_var) < 1e-9

    # --- count_above() ---------------------------------------------------
    print_header("count_above(data, limit)")
    limit = 24.0
    count = sensor_analysis.count_above(readings, limit)
    print(f"count_above(data, {limit}) = {count}")
    assert count == sum(1 for x in readings if x > limit)

    # Test with a tuple instead of a list, to confirm both are accepted.
    tuple_readings = tuple(readings)
    count_tuple = sensor_analysis.count_above(tuple_readings, limit)
    assert count_tuple == count
    print(f"count_above() also works with a tuple: {count_tuple}")

    # --- statistics() --------------------------------------------------
    print_header("statistics(data)")
    stats = sensor_analysis.statistics(readings)
    print(f"statistics(data) = {stats}")
    assert stats["samples"] == len(readings)
    assert abs(stats["average"] - avg) < 1e-9
    assert stats["minimum"] == min(readings)
    assert stats["maximum"] == max(readings)

    # --- Invalid input: wrong type ---------------------------------------
    print_header("Invalid input: wrong type (string instead of list/tuple)")
    try:
        sensor_analysis.average("not a sequence of numbers")
        print("FAIL: expected TypeError, none was raised")
    except TypeError as e:
        print(f"Correctly raised TypeError: {e}")

    # --- Invalid input: empty dataset -------------------------------------
    print_header("Invalid input: empty dataset")
    try:
        sensor_analysis.average([])
        print("FAIL: expected ValueError, none was raised")
    except ValueError as e:
        print(f"Correctly raised ValueError: {e}")

    # --- Invalid input: non-numeric element in an otherwise valid list ----
    print_header("Invalid input: non-numeric element in the list")
    try:
        sensor_analysis.average([1.0, 2.0, "three", 4.0])
        print("FAIL: expected TypeError, none was raised")
    except TypeError as e:
        print(f"Correctly raised TypeError: {e}")

    # --- Boundary condition: variance needs at least 2 data points --------
    print_header("Boundary condition: variance() with a single data point")
    try:
        sensor_analysis.variance([42.0])
        print("FAIL: expected ValueError, none was raised")
    except ValueError as e:
        print(f"Correctly raised ValueError: {e}")

    print_header("All tests completed")


if __name__ == "__main__":
    main()