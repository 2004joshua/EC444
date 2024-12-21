# Code Readme

## Code Structure

```
code
├── esp
│   ├── CMakeLists.txt
│   ├── main
│   │   ├── ADXL343.h          - Defines for Accel Chip
│   │   ├── char_binary.h      - Converts text into binary
│   │   ├── CMakeLists.txt
│   │   ├── cat_track.c        - Cat Track entry point
│   │   └── pins.h             - Pin mappings for ESP32
│   ├── sdkconfig
│   ├── sdkconfig.ci
│   └── sdkconfig.old
├── node
│   ├── data-server
│   │   ├── package.json
│   │   ├── package-lock.json
│   │   └── src
│   │       └── index.js       - Entry point for serial to socket conversion
│   └── web-server
│       ├── package.json
│       ├── package-lock.json
│       ├── public             - Website with charts
│       │   ├── index.css
│       │   ├── index.html
│       │   └── index.js
│       └── src
│           └── index.js       - Entry point for web server
└── README.md - This file
```

