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
#define CONFIG_STEER_LOOKAHEAD_FAR_START 60
#define CONFIG_STEER_LOOKAHEAD_FAR_END 80
#define CONFIG_STEER_LOOKAHEAD_GAIN 0.35f
#define CONFIG_STEER_LOOKAHEAD_MAX_OFFSET 10
#define CONFIG_STEER_LOOKAHEAD_MAX_HEADING 25
#define CONFIG_STEER_LOOKAHEAD_FILTER_OLD 0.3f
#define CONFIG_STEER_LOOKAHEAD_SPEED_MID 115
#define CONFIG_STEER_LOOKAHEAD_SPEED_HIGH 140
#define CONFIG_STEER_LOOKAHEAD_MID_FAR_START 55
#define CONFIG_STEER_LOOKAHEAD_MID_FAR_END 75
#define CONFIG_STEER_LOOKAHEAD_MID_GAIN 0.45f
#define CONFIG_STEER_LOOKAHEAD_MID_MAX_OFFSET 12
#define CONFIG_STEER_LOOKAHEAD_MID_FILTER_OLD 0.2f
#define CONFIG_STEER_LOOKAHEAD_HIGH_FAR_START 45
#define CONFIG_STEER_LOOKAHEAD_HIGH_FAR_END 65
#define CONFIG_STEER_LOOKAHEAD_HIGH_GAIN 0.55f
#define CONFIG_STEER_LOOKAHEAD_HIGH_MAX_OFFSET 15
#define CONFIG_STEER_LOOKAHEAD_HIGH_FILTER_OLD 0.1f
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
    steer_lookahead_profile profile;
    int failed = 0;

    profile = steer_lookahead_select_profile(100);
    failed += expect("low_far_start", profile.far_start, 60);
    failed += expect("low_far_end", profile.far_end, 80);
    failed += expect("low_gain", (int)(profile.gain * 100.0f + 0.5f), 35);

    profile = steer_lookahead_select_profile(120);
    failed += expect("mid_far_start", profile.far_start, 55);
    failed += expect("mid_far_end", profile.far_end, 75);
    failed += expect("mid_gain", (int)(profile.gain * 100.0f + 0.5f), 45);

    profile = steer_lookahead_select_profile(150);
    failed += expect("high_far_start", profile.far_start, 45);
    failed += expect("high_far_end", profile.far_end, 65);
    failed += expect("high_gain", (int)(profile.gain * 100.0f + 0.5f), 55);

    profile = steer_lookahead_select_profile(100);
    failed += expect("straight", steer_lookahead_calculate(93, 93, 93, 93, profile), 93);
    failed += expect("low_right_curve", steer_lookahead_calculate(93, 93, 103, 93, profile), 95);
    failed += expect("left_curve", steer_lookahead_calculate(93, 93, 83, 93, profile), 90);
    failed += expect("abnormal_heading", steer_lookahead_calculate(93, 93, 120, 93, profile), 93);
    failed += expect("upper_bound", steer_lookahead_calculate(185, 93, 103, 186, profile), 186);
    failed += expect("lower_bound", steer_lookahead_calculate(1, 93, 83, 1, profile), 1);
    failed += expect("low_pass", steer_lookahead_calculate(100, 93, 93, 80, profile), 94);

    profile = steer_lookahead_select_profile(120);
    failed += expect("mid_right_curve", steer_lookahead_calculate(93, 93, 103, 93, profile), 96);
    profile = steer_lookahead_select_profile(150);
    failed += expect("high_right_curve", steer_lookahead_calculate(93, 93, 103, 93, profile), 97);

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
