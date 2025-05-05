#!/bin/bash

print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Recursively format all .c and .h files using clang-format"
    echo ""
    echo "Options:"
    echo "  -h, --help      Show this help message"
    echo "  -d, --dry-run   Show which files would be formatted without actually changing them"
    echo "  -s, --style     Specify clang-format style (default: file)"
    echo "  -v, --verbose   Print more information during execution"
}

DRY_RUN=0
STYLE="file"
VERBOSE=0
ROOT_DIR="$(pwd)"
EXCLUDE_DIR="scripts"

# Process command-line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            print_usage
            exit 0
            ;;
        -d|--dry-run)
            DRY_RUN=1
            shift
            ;;
        -s|--style)
            STYLE="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        *)
            echo "Error: Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

if ! command -v clang-format &> /dev/null; then
    echo "Error: clang-format is not installed or not in PATH"
    echo "Please install clang-format and try again"
    exit 1
fi

echo "Starting code formatting..."
echo "Root directory: $ROOT_DIR"
echo "Excluding directory: $EXCLUDE_DIR"
echo "Style: $STYLE"
if [ $DRY_RUN -eq 1 ]; then
    echo "Dry run: no files will be modified"
fi
echo ""

FOUND_FILES=0
FORMATTED_FILES=0

while IFS= read -r file; do
    ((FOUND_FILES++))

    if [ $VERBOSE -eq 1 ]; then
        echo "Processing: $file"
    fi

    if [ $DRY_RUN -eq 1 ]; then
        echo "Would format: $file"
    else
        if [ $VERBOSE -eq 1 ]; then
            echo "Formatting: $file"
        fi
        clang-format -i --style="$STYLE" "$file"
        if [ $? -eq 0 ]; then
            ((FORMATTED_FILES++))
        else
            echo "Error formatting: $file"
        fi
    fi
done < <(find "$ROOT_DIR" -type f \( -name "*.c" -o -name "*.h" \) -not -path "*/$EXCLUDE_DIR/*")

# Print summary
echo ""
echo "Formatting complete!"
echo "Found $FOUND_FILES file(s)"
if [ $DRY_RUN -eq 1 ]; then
    echo "Would have formatted $FOUND_FILES file(s)"
else
    echo "Successfully formatted $FORMATTED_FILES file(s)"
fi

exit 0
