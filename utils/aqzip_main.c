#include "aqzip.h"

#if CPA_DC_API_VERSION_AT_LEAST(3,0)
#define COMP_LVL_MAXIMUM QZ_LZS_COMP_LVL_MAXIMUM
#else
#define COMP_LVL_MAXIMUM QZ_DEFLATE_COMP_LVL_MAXIMUM
#endif

int main(int argc, char **argv)
{
    int ret = QZ_OK;
    int arg_count; /* number of files or directories to process */
    g_program_name = aqzipBaseName(argv[0]);
    char *out_name = NULL;
    // FILE *stream_out = NULL;
    int option_f = 0;
    // int recursive_mode = 0;
    int is_dir = 0;
    //int chain_mode = 0;
    //int hash_mode = 0;
    int sw_mode = 0;
    errno = 0;
    int demo = 0;

    if (qzGetDefaults(&g_qz_params_th) != QZ_OK)
        return -1;
    
    if (aqzGetDefaults(&g_aqz_params_th) != QZ_OK)
        return -1;

    if (aqzGetInitDefaults(&g_init_prarams_th) != OK)
        return -1;

    while (true) {
        int optc;
        int long_idx = -1;
        char *stop = NULL;

        optc = getopt_long(argc, argv, g_short_opts, g_long_opts, &long_idx);
        if (optc < 0) {
            break;
        }
        switch (optc) {
        case 'd':
            g_decompress = 1;
            break;
        case 'h':
            help();
            exit(OK);
            break;
        case 'k':
            g_keep = 1;
            break;
        case 'R':
            // recursive_mode = 1;
            break;
        case 'A':
            if (strcmp(optarg, "deflate") == 0) {
                g_qz_params_th.comp_algorithm = QZ_DEFLATE;
                } else if (strcmp(optarg, "lz4") == 0) {
                g_qz_params_th.comp_algorithm = QZ_LZ4;
            } else if (strcmp(optarg, "lz4s") == 0) {
                g_qz_params_th.comp_algorithm = QZ_LZ4s;
            } else {
                QZ_ERROR("Error service arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'H':
            if (strcmp(optarg, "static") == 0) {
                g_qz_params_th.huffman_hdr = QZ_STATIC_HDR;
            } else if (strcmp(optarg, "dynamic") == 0) {
                g_qz_params_th.huffman_hdr = QZ_DYNAMIC_HDR;
            } else {
                QZ_ERROR("Error huffman arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'o':
            out_name = optarg;
            break;
        case 'L':
            g_qz_params_th.comp_lvl = GET_LOWER_32BITS(strtoul(optarg, &stop, 0));
            if (*stop != '\0' || ERANGE == errno ||
                g_qz_params_th.comp_lvl > COMP_LVL_MAXIMUM || g_qz_params_th.comp_lvl <= 0) {
                QZ_ERROR("Error compLevel arg: %s\n", optarg);
                return -1;
            }
            break;
        /*case 'Y':
            hash_mode = 1;
            break;
        case 'c':
            chain_mode = 1;
            break;*/
        case 'S':
            sw_mode = 1;
            break;
        case 'C':
            g_qz_params_th.hw_buff_sz =
                GET_LOWER_32BITS(strtoul(optarg, &stop, 0));
            if (*stop != '\0' || ERANGE == errno ||
                g_qz_params_th.hw_buff_sz > USDM_ALLOC_MAX_SZ / 2) {
                printf("Error chunk size arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'r':
            g_qz_params_th.req_cnt_thrshold =
                GET_LOWER_32BITS(strtoul(optarg, &stop, 0));
            if (*stop != '\0' || errno ||
                g_qz_params_th.req_cnt_thrshold > NUM_BUFF) {
                printf("Error request count threshold: %s\n", optarg);
                return -1;
            }
            break;
        case 'O':
            if (strcmp(optarg, "gzip") == 0) {
                g_qz_params_th.data_fmt = QZ_DEFLATE_GZIP;
            } else if (strcmp(optarg, "gzipext") == 0) {
                g_qz_params_th.data_fmt = QZ_DEFLATE_GZIP_EXT;
            } else if (strcmp(optarg, "7z") == 0) {
                g_qz_params_th.data_fmt = QZ_DEFLATE_RAW;
                } else if (strcmp(optarg, "deflate_4B") == 0) {
                g_qz_params_th.data_fmt = QZ_DEFLATE_4B;
            /*} else if (strcmp(optarg, "lz4") == 0) {
                g_qz_params_th.data_fmt = QZ_LZ4_FH;
            } else if (strcmp(optarg, "lz4s") == 0) {
                g_qz_params_th.data_fmt = QZ_LZ4S_BK;*/
            } else {
                QZ_ERROR("Error gzip header format arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'f':
            option_f = 1;
            break;
        case 'D':
            demo = 1;
            break;
        default:
            // tryHelp();
            break;
        }
    }

    arg_count = argc - optind;
    if (0 == arg_count && isatty(fileno((FILE *)stdin))) {
        help();
        exit(OK);
    }

    /*if (chain_mode == 1) {
        g_sess.func_mode = 1;
        if (1 == g_decompress) {
            g_qz_params_th.direction = QZ_DIR_DECOMPRESS;
        } else {
            g_qz_params_th.direction = QZ_DIR_COMPRESS;
        }
        // g_qz_params_th.direction = QZ_DIR_BOTH;
    } else if (hash_mode == 1) {
        g_sess.func_mode = QZ_FUNC_HASH;
    } else {
        g_qz_params_th.direction = QZ_DIR_BOTH;
    }*/
    g_qz_params_th.direction = QZ_DIR_BOTH;

    if (sw_mode == 1) {
        g_init_prarams_th.sw_mode = CPA_TRUE;
    }

    if (aqatzipSetup(&g_sess, &g_init_prarams_th, &g_qz_params_th, &g_aqz_params_th)) {
        exit(ERROR);
    }

    if (0 == arg_count) {
        if (1 == demo) {
            /*if (1 == chain_mode) {
                chainCompressAndDecompressSample(&g_sess);
            } else {
                compressAndDecompressSample(&g_sess);
            }*/
            compressAndDecompressSample(&g_sess);
            
        } else if (isatty(fileno((FILE *)stdout)) && 0 == option_f &&
            0 == g_decompress) {
            printf("qzip: compressed data not written to a terminal. "
                   "Use -f to force compression.\n");
            printf("For help, type: qzip -h\n");
        }
    } else {
        while (optind < argc) {

            if (access(argv[optind], F_OK)) {
                QZ_ERROR("%s: No such file or directory\n", argv[optind]);
                exit(ERROR);
            }

            is_dir = checkDirectory(argv[optind]);

            if (g_decompress && !is_dir && !hasSuffix(argv[optind])) {
                QZ_ERROR("Error: %s: Wrong suffix. Supported suffix: 7z/gz.\n",
                         argv[optind]);
                exit(ERROR);
            }

            processFile(&g_sess, argv[optind++], out_name,
                        g_decompress == 0);
        }
    }

    if (aqatzipClose(&g_sess)) {
        exit(ERROR);
    }

    return ret;
}