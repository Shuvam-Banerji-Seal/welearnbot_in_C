# Interactive Mode Guide

This guide explains the new interactive file selection features added to the WeLearn CLI downloader.

## Overview

The WeLearn CLI downloader now supports two modes of operation:
1. **Automatic Mode** - Downloads all files immediately (original behavior)
2. **Interactive Mode** - Scan, browse, and selectively download files

## Interactive Mode Features

### 1. File Scanning

Instead of downloading immediately, the tool first scans all your courses and collects information about available files:

```
Scanning courses and collecting file information...
Scanning course: https://welearn.iiserkol.ac.in/course/view.php?id=123
  Found course: Data Structures and Algorithms
Scanning course: https://welearn.iiserkol.ac.in/course/view.php?id=124
  Found course: Operating Systems
--- Scan Complete: Found 45 file(s) ---
```

### 2. Display Options

#### Tree View
Shows files in a hierarchical structure with visual indicators:

```
========================================
Files Available for Download (Tree View)
========================================

📚 Course: Data Structures and Algorithms
📄 [1] Syllabus.pdf
📄 [2] Lecture_1_Introduction.pdf
📁 [3] Week_2_Resources (folder)
  📄 [4] Lecture_2_Arrays.pdf
  📄 [5] Assignment_1.pdf
📁 [6] Practice_Problems (folder)
  📄 [7] Problems_Set_1.pdf

📚 Course: Operating Systems
📄 [8] Course_Overview.pdf
📄 [9] Chapter_1_Introduction.pdf
========================================
```

#### List View
Shows files in a simple tabular format:

```
========================================
Files Available for Download (List View)
========================================
No.   Course                         Filename
----------------------------------------
[1  ] Data Structures and Algorith   Syllabus.pdf
[2  ] Data Structures and Algorith   Lecture_1_Introduction.pdf
[3  ] Data Structures and Algorith   Week_2_Resources (folder)
[4  ] Data Structures and Algorith   Lecture_2_Arrays.pdf
[5  ] Data Structures and Algorith   Assignment_1.pdf
[6  ] Data Structures and Algorith   Practice_Problems (folder)
[7  ] Data Structures and Algorith   Problems_Set_1.pdf
[8  ] Operating Systems               Course_Overview.pdf
[9  ] Operating Systems               Chapter_1_Introduction.pdf
========================================
Total: 9 file(s)
```

### 3. Custom Download Directory

You can specify where files should be saved:

```
Enter download directory path (press Enter for current directory '.'): ./my_courses
```

- Press Enter to use the current directory
- Specify any path (will be created if it doesn't exist)
- Files are organized into subdirectories by course name

### 4. File Selection

Multiple ways to select files for download:

#### Download All Files
```
Your selection: all
```

#### Download Specific Files
```
Your selection: 1,3,5,7
```
Downloads files numbered 1, 3, 5, and 7.

#### Download Range (manual input)
```
Your selection: 1,2,3,4,5,6
```

#### Quit Without Downloading
```
Your selection: q
```

## Complete Usage Example

### Step-by-Step Walkthrough

1. **Start the application**
   ```bash
   ./welearn_cli
   ```

2. **Login** (if credentials not saved)
   ```
   Credentials not found or failed to load.
   Please enter your WeLearn credentials:
   Username: student@iiserkol.ac.in
   Password: ********
   Save credentials for future use? (y/n): y
   ```

3. **Choose mode**
   ```
   ===========================================
     WeLearn File Download Manager
   ===========================================

   Choose an option:
   1. Download all files (old behavior)
   2. Select specific files to download (new)

   Enter choice (1 or 2): 2
   ```

4. **Wait for scanning**
   ```
   Scanning courses and collecting file information...
   ...
   --- Scan Complete: Found 45 file(s) ---
   ```

5. **Choose display format**
   ```
   How would you like to view the files?
   1. Tree view (hierarchical)
   2. List view (simple table)

   Enter choice (1 or 2): 1
   ```

6. **View files** (tree view shown)
   ```
   ========================================
   Files Available for Download (Tree View)
   ========================================
   [files displayed here]
   ```

7. **Specify download directory**
   ```
   Enter download directory path (press Enter for current directory '.'): ./downloads
   ```

8. **Select files to download**
   ```
   Select files to download:
     - Enter 'all' to download all files
     - Enter file numbers separated by commas (e.g., 1,3,5,7)
     - Enter 'q' to quit without downloading

   Your selection: 1,2,8,9
   ```

9. **Download process**
   ```
   Preparing to download 4 file(s)...

   --- Starting Downloads ---
   Download location: ./downloads

   [1/4] Downloading: Syllabus.pdf
   Attempting to download resource: https://...
   --> Using filename from header: Syllabus.pdf
   Successfully downloaded: ./downloads/Data_Structures_and_Algorithms/Syllabus.pdf

   [2/4] Downloading: Lecture_1_Introduction.pdf
   ...

   --- Downloads Complete ---
   ```

## Tips and Best Practices

### Efficient File Selection

1. **Preview before downloading**: Use the scan mode to see what's available before committing to downloads
2. **Organize by directory**: Specify a dedicated download directory to keep files organized
3. **Selective downloads**: Only download new materials instead of re-downloading everything
4. **Use tree view for complex courses**: Tree view makes it easier to see folder structures

### Directory Organization

Downloaded files are automatically organized:
```
downloads/
├── Data_Structures_and_Algorithms/
│   ├── Syllabus.pdf
│   ├── Lecture_1_Introduction.pdf
│   └── Lecture_2_Arrays.pdf
└── Operating_Systems/
    ├── Course_Overview.pdf
    └── Chapter_1_Introduction.pdf
```

### Handling Large Numbers of Files

When dealing with many files:
1. Use list view for easier reference of file numbers
2. Note down the numbers you want before entering them
3. Use "all" if you need everything
4. Download in batches if needed (run the tool multiple times)

## Backward Compatibility

The original behavior (Mode 1) is fully preserved:
- Automatically downloads all files
- No user interaction required
- Saves to current directory
- Useful for complete course backups

## Troubleshooting

### No files found after scanning
- Check your internet connection
- Verify you're enrolled in courses
- Ensure WeLearn website is accessible

### Invalid selection error
- Ensure you're entering valid file numbers
- Use commas to separate numbers: `1,3,5` not `1 3 5`
- Check that numbers are within the displayed range

### Directory creation failed
- Verify you have write permissions
- Check disk space
- Ensure path is valid for your operating system

## Comparison: Old vs New Mode

| Feature | Mode 1 (Old) | Mode 2 (New) |
|---------|--------------|--------------|
| File preview | No | Yes |
| Selective download | No | Yes |
| Custom directory | No | Yes |
| Display format | N/A | Tree or List |
| User control | Minimal | Full |
| Speed | Immediate | Scan first |
| Use case | Complete backup | Selective updates |

## Advanced Usage

### Scripting Mode Selection

You can automate mode selection by piping input:
```bash
echo "1" | ./welearn_cli  # Always use Mode 1
```

For interactive mode with pre-selected files:
```bash
echo -e "2\n1\n./downloads\n1,2,3" | ./welearn_cli
```
This selects:
- Mode 2 (interactive)
- View 1 (tree)
- Directory: ./downloads
- Files: 1, 2, 3

### Integration with Other Tools

The file list format is designed to be parse-friendly for potential future enhancements:
- JSON export of file list
- Filter by course or file type
- Search functionality
- Bookmark favorite files

## Future Enhancements

Planned improvements:
- [ ] Save and load file selections
- [ ] Search/filter files by name or course
- [ ] Download queue management
- [ ] Resume interrupted downloads
- [ ] Export file list to text/CSV
