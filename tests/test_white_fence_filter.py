from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
IMAGE_C = ROOT / "code" / "image.c"

source = IMAGE_C.read_bytes().decode("gbk")
match = re.search(
    r"/\* LINE_FILTER_TEST_BEGIN \*/(.*?)/\* LINE_FILTER_TEST_END \*/",
    source,
    re.S,
)
if not match:
    raise AssertionError("production line filter function not found")

harness = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t uint8;
#define MT9V03X_W 188
#define CONFIG_LINE_MAX_BLACK_GAP 3
#define CONFIG_LINE_MAX_MID_JUMP 6
#define CONFIG_LINE_MAX_WIDTH_JUMP 12
''' + match.group(1) + r'''

static void white_segment(uint8 *row, int left, int right)
{
    for(int i = left; i <= right; ++i) row[i] = 255;
}

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
    uint8 row[MT9V03X_W];
    int failed = 0;

    memset(row, 0, sizeof(row));
    white_segment(row, 40, 140);
    failed += expect("normal", line_candidate_is_valid(row, 40, 140, 40, 140, 1), 1);

    memset(row, 0, sizeof(row));
    white_segment(row, 30, 80);
    white_segment(row, 90, 150);
    failed += expect("black_gap", line_candidate_is_valid(row, 30, 150, 30, 150, 0), 0);

    memset(row, 0, sizeof(row));
    white_segment(row, 90, 150);
    failed += expect("adjacent_track", line_candidate_is_valid(row, 90, 150, 30, 80, 1), 0);

    memset(row, 0, sizeof(row));
    white_segment(row, 43, 142);
    failed += expect("normal_curve", line_candidate_is_valid(row, 43, 142, 40, 140, 1), 1);

    memset(row, 0, sizeof(row));
    white_segment(row, 35, 155);
    failed += expect("width_jump", line_candidate_is_valid(row, 35, 155, 40, 140, 1), 0);

    if(failed == 0) printf("PASS white fence filter\n");
    return failed != 0;
}
'''

with tempfile.TemporaryDirectory(prefix="white-fence-filter-") as temp_dir:
    temp = Path(temp_dir)
    source_file = temp / "line_filter_test.c"
    exe_file = temp / "line_filter_test.exe"
    source_file.write_text(harness, encoding="utf-8")
    subprocess.run(
        ["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror", str(source_file), "-o", str(exe_file)],
        check=True,
    )
    result = subprocess.run([str(exe_file)], check=True, text=True, capture_output=True)
    if "PASS white fence filter" not in result.stdout:
        raise AssertionError(result.stdout)
    print(result.stdout.strip())
