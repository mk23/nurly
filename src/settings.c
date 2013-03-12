#include "nurly.h"

int nurly_config_read(char* cfg_name, nurly_config_t* nurly_config) {
    int       parse_error = FALSE;
	char*     cfg_key = NULL;
	char*     cfg_val = NULL;
    char*     cfg_line = NULL;
    char*     temp_ptr = NULL;
    mmapfile* cfg_file = NULL;

    if ((cfg_file = mmap_fopen(cfg_name)) == NULL) {
        nurly_log("error: unable to open configuration file: %s", cfg_name);
        return ERROR;
    } else {
        nurly_log("processing configuration file: %s", cfg_name);
    }

    while (TRUE) {
        NURLY_FREE(cfg_line);
        NURLY_FREE(cfg_key);
        NURLY_FREE(cfg_val);

        if ((cfg_line = mmap_fgets_multiline(cfg_file)) == NULL) {
            nurly_log("end of configuration file");
            break;
        }

        strip(cfg_line);

        if (cfg_line[0] == '\x0' || cfg_line[0] == '#') {
            continue;
        }

        /* extract key */
        if ((temp_ptr = my_strtok(cfg_line, "=")) == NULL) {
            nurly_log("warning: invalid configuration on line %2d: %s", cfg_file->current_line, cfg_line);
            continue;
        }
        if ((cfg_key = (char*)strdup(temp_ptr)) == NULL) {
            nurly_log("error: unable to allocate memory for configuration key");
            parse_error = TRUE;
            break;
        }
        /* extract value */
        if ((temp_ptr = my_strtok(NULL, "\n")) == NULL) {
            nurly_log("warning: invalid configuration on line %2d: %s", cfg_file->current_line, cfg_line);
            continue;
        }
        if ((cfg_val = (char*)strdup(temp_ptr)) == NULL) {
            nurly_log("error: unable to allocate memory for configuration value");
            parse_error = TRUE;
            break;
        }

        strip(cfg_key);
        strip(cfg_val);

        if (!strcmp(cfg_key, "checks_url")) {
            if ((nurly_config->checks_url = (char*)strdup(cfg_val)) == NULL) {
                nurly_log("error: unable to allocate memory for checks_url value");
                parse_error = TRUE;
                break;
            } else {
                nurly_log("set checks url: %s", nurly_config->checks_url);
            }
        } else if (!strcmp(cfg_key, "health_url")) {
            if ((nurly_config->health_url = (char*)strdup(cfg_val)) == NULL) {
                nurly_log("error: unable to allocate memory for health_url value");
                parse_error = TRUE;
                break;
            } else {
                nurly_log("set health url: %s", nurly_config->health_url);
            }
        } else if (!strcmp(cfg_key, "health_interval")) {
            if ((nurly_config->health_interval = atoi(cfg_val)) == 0) {
                nurly_log("warning: invalid health_interval value");
                nurly_config->health_interval = 2;
            }
            nurly_log("set health interval: %d", nurly_config->health_interval);
        } else if (!strcmp(cfg_key, "worker_threads")) {
            if ((nurly_config->worker_threads = atoi(cfg_val)) == 0) {
                nurly_log("warning: invalid worker_threads value");
                nurly_config->worker_threads = 20;
            }
            nurly_log("set worker threads: %d", nurly_config->worker_threads);
        } else {
            nurly_log("warning: unknown configuration key: %s", cfg_key);
        }
    }

    NURLY_FREE(cfg_key);
    NURLY_FREE(cfg_val);
    NURLY_FREE(cfg_line);
	mmap_fclose(cfg_file);

    return parse_error == TRUE ? ERROR : OK;
}

void nurly_config_free(nurly_config_t* nurly_config) {
    NURLY_FREE(nurly_config->checks_url);
    NURLY_FREE(nurly_config->health_url);
}
