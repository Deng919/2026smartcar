from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
IMAGE_C = ROOT / "code" / "image.c"

source = IMAGE_C.read_bytes().decode("gbk")
match = re.search(
    r"/\* STEERING_LOOKAHEAD_TEST_BEGIN \*/(.*?)/\* STEERING_LOOKAHEAD_TEST_END \*/",
    source,
    re.S,
)
if not match:
    raise AssertionError("production steering lookahead function not found")

harness = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t uint8;

#define MT9V03X_H 120
#define MT9V03X_W 188
#define CONFIG_STEER_LOOKAHEAD_GAIN 0.35f
#define CONFIG_STEER_LOOKAHEAD_MAX_OFFSET 10
#define CONFIG_STEER_LOOKAHEAD_MAX_HEADING 25
#define CONFIG_STEER_LOOKAHEAD_FILTER_OLD 0.3f
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
    uint8 lines[MT9V03X_H];
    int failed = 0;

    failed += expect("straight", steer_lookahead_calculate(93, 93, 93, 93), 93);
    failed += expect("right_curve", steer_lookahead_calculate(93, 93, 103, 93), 95);
    failed += expect("left_curve", steer_lookahead_calculate(93, 93, 83, 93), 90);
    failed += expect("abnormal_heading", steer_lookahead_calculate(93, 93, 120, 93), 93);
    failed += expect("upper_bound", steer_lookahead_calculate(185, 93, 103, 186), 186);
    failed += expect("lower_bound", steer_lookahead_calculate(1, 93, 83, 1), 1);
    failed += expect("low_pass", steer_lookahead_calculate(100, 93, 93, 80), 94);

    memset(lines, 0, sizeof(lines));
    for(int row = 60; row <= 80; ++row) lines[row] = 100;
    for(int row = 96; row <= 115; ++row) lines[row] = 90;
    failed += expect("far_average", image_mid_line_average(lines, 60, 80), 100);
    failed += expect("near_average", image_mid_line_average(lines, 96, 115), 90);

    if(failed == 0)
    {
        printf("PASS steering lookahead\n");
    }
    return failed != 0;
}
'''

with tempfile.TemporaryDirectory(prefix="steering-lookahead-") as temp_dir:
    temp = Path(temp_dir)
    source_file = temp / "steering_lookahead_test.c"
    exe_file = temp / "steering_lookahead_test.exe"
    source_file.write_text(harness, encoding="utf-8")
    subprocess.run(
        ["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror", str(source_file), "-o", str(exe_file)],
        check=True,
    )
    result = subprocess.run([str(exe_file)], check=True, text=True, capture_output=True)
    if "PASS steering lookahead" not in result.stdout:
        raise AssertionError(result.stdout)
    print(result.stdout.strip())
