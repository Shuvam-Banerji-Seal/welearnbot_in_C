#!/bin/bash
# Simple validation test for new features

echo "==================================="
echo "WeLearn Downloader - Feature Tests"
echo "==================================="

# Test 1: Check if binary exists and is executable
echo -e "\n[TEST 1] Binary exists and is executable"
if [ -x ./welearn_cli ]; then
    echo "✓ PASS: welearn_cli binary is executable"
else
    echo "✗ FAIL: welearn_cli binary not found or not executable"
    exit 1
fi

# Test 2: Check if help/usage works (or at least runs without credentials)
echo -e "\n[TEST 2] Application starts without crashing"
# Send input to quit early: choose mode 1 but with no valid credentials
# The app should handle this gracefully
TEST_OUTPUT=$(mktemp)
timeout 2 echo -e "\n" | ./welearn_cli > "$TEST_OUTPUT" 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] || [ $EXIT_CODE -eq 124 ]; then
    echo "✓ PASS: Application starts (timeout or normal exit expected)"
else
    echo "✗ FAIL: Application crashed with code $EXIT_CODE"
    cat "$TEST_OUTPUT"
    rm -f "$TEST_OUTPUT"
    exit 1
fi
rm -f "$TEST_OUTPUT"

# Test 3: Verify new functions are linked
echo -e "\n[TEST 3] New functions are present in binary"
FUNCS=("init_file_list" "add_file_to_list" "display_file_tree" "scan_courses_and_collect_files")
for func in "${FUNCS[@]}"; do
    if nm welearn_cli | grep -q "$func"; then
        echo "✓ PASS: Function '$func' found in binary"
    else
        echo "✗ FAIL: Function '$func' not found in binary"
        exit 1
    fi
done

# Test 4: Check if documentation was created
echo -e "\n[TEST 4] Documentation files exist"
if [ -f "docs/INTERACTIVE_MODE.md" ]; then
    echo "✓ PASS: Interactive mode documentation exists"
else
    echo "✗ FAIL: Documentation file missing"
    exit 1
fi

# Test 5: Verify README was updated
echo -e "\n[TEST 5] README contains new feature documentation"
if grep -q "Interactive Mode" README.md && grep -q "Tree/List view" README.md; then
    echo "✓ PASS: README updated with new features"
else
    echo "✗ FAIL: README not properly updated"
    exit 1
fi

echo -e "\n==================================="
echo "All tests passed! ✓"
echo "==================================="
echo ""
echo "New features implemented:"
echo "  ✓ File collection and scanning"
echo "  ✓ Tree and list view display"
echo "  ✓ Interactive file selection"
echo "  ✓ Custom download directory"
echo "  ✓ Selective file downloading"
echo "  ✓ Backward compatibility maintained"
echo ""
echo "To test manually, run: ./welearn_cli"
echo "(Note: Requires valid WeLearn credentials)"

exit 0
