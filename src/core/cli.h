#pragma once

// CLI argument parsing macros

#define ARG_BOOL(opt, var) \
    if (arg == opt) { var = true; continue; }

#define ARG_INT(opt, var, err) \
    if (arg == opt) { \
        if (++i >= argc) { \
            printf("%s\n", err); \
            return 1; \
        } \
        var = atoi(argv[i]); \
        continue; \
    }

#define ARG_FLT(opt, var, err) \
    if (arg == opt) { \
        if (++i >= argc) { \
            printf("%s\n", err); \
            return 1; \
        } \
        var = atof(argv[i]); \
        continue; \
    }

#define ARG_VAL(opt, var, err) \
    if (arg == opt) { \
        if (++i >= argc) { \
            printf("%s\n", err); \
            return 1; \
        } \
        var = argv[i]; \
        continue; \
    }
