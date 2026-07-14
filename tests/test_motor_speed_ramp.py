from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
MOTOR_C = ROOT / "code" / "motor.c"


def read_source(path: Path) -> str:
    data = path.read_bytes()
    for encoding in ("utf-8", "gbk"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            pass
    raise AssertionError(f"cannot decode {path}")


source = read_source(MOTOR_C)
match = re.search(
    r"/\* MOTOR_SPEED_RAMP_TEST_BEGIN \*/(.*?)/\* MOTOR_SPEED_RAMP_TEST_END \*/",
    source,
    re.S,
)
if not match:
    raise AssertionError("production motor speed ramp function not found")

harness = r'''
#include <stdio.h>

#define CONFIG_MOTOR_ACCEL_STEP 2
#define CONFIG_MOTOR_DECEL_STEP 4
#define CONFIG_MOTOR_SPEED_LIMIT 25
''' + match.group(1) + r'''

static int expect(const char *name, int actual, int expected)
{
    if(actual != expected)
    {
        printf("FAIL %s actual=%d expected=%d\n", name, actual, expected);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failed = 0;
    int speed = 0;

    speed = motor_target_speed_ramp_next(speed, 10);
    failed += expect("accelerate_1", speed, 2);
    speed = motor_target_speed_ramp_next(speed, 10);
    failed += expect("accelerate_2", speed, 4);
    speed = motor_target_speed_ramp_next(speed, 10);
    failed += expect("accelerate_3", speed, 6);
    speed = motor_target_speed_ramp_next(speed, 10);
    failed += expect("accelerate_4", speed, 8);
    speed = motor_target_speed_ramp_next(speed, 10);
    failed += expect("accelerate_clamp", speed, 10);

    speed = motor_target_speed_ramp_next(speed, 0);
    failed += expect("decelerate_1", speed, 6);
    speed = motor_target_speed_ramp_next(speed, 0);
    failed += expect("decelerate_2", speed, 2);
    speed = motor_target_speed_ramp_next(speed, 0);
    failed += expect("decelerate_clamp", speed, 0);

    failed += expect("hold", motor_target_speed_ramp_next(7, 7), 7);
    failed += expect("small_accel_gap", motor_target_speed_ramp_next(9, 10), 10);
    failed += expect("small_decel_gap", motor_target_speed_ramp_next(3, 0), 0);
    failed += expect("zero_diff_limit", motor_speed_ramp_diff_limit(0), 0);
    failed += expect("startup_diff_limit", motor_speed_ramp_diff_limit(2), 2);
    failed += expect("normal_diff_limit", motor_speed_ramp_diff_limit(40), 25);

    if(failed == 0)
    {
        printf("PASS motor speed ramp\n");
    }
    return failed != 0;
}
'''

with tempfile.TemporaryDirectory(prefix="motor-speed-ramp-") as temp_dir:
    temp = Path(temp_dir)
    source_file = temp / "motor_speed_ramp_test.c"
    exe_file = temp / "motor_speed_ramp_test.exe"
    source_file.write_text(harness, encoding="utf-8")
    subprocess.run(
        ["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror", str(source_file), "-o", str(exe_file)],
        check=True,
    )
    result = subprocess.run([str(exe_file)], check=True, text=True, capture_output=True)
    if "PASS motor speed ramp" not in result.stdout:
        raise AssertionError(result.stdout)
    print(result.stdout.strip())
