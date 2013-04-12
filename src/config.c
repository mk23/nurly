#include "nurly.h"

static void nurly_log_regerror(int err, regex_t* reg, char* pat) {
    size_t len = regerror(err, reg, NULL, 0);
    char*  buf = NULL;

    if ((buf = (char*)malloc(len)) == NULL) {
        nurly_log("error: unable to allocate memory for regerror");
    } else {
        regerror(err, reg, buf, len);
        nurly_log("error: unable to compile pattern for %s: %s", pat, buf);
    }

    regfree(reg);
    NURLY_FREE(reg);
    NURLY_FREE(buf);
}

int nurly_config_read(char* cfg_name, nurly_config_t* nurly_config) {
    int       parse_error = FALSE;
    char*     cfg_key = NULL;
    char*     cfg_val = NULL;
    char*     cfg_line = NULL;
    char*     temp_ptr = NULL;
    regex_t*  temp_reg = NULL;
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
        if ((cfg_key = strdup(temp_ptr)) == NULL) {
            nurly_log("error: unable to allocate memory for configuration key");
            parse_error = TRUE;
            break;
        }
        /* extract value */
        if ((temp_ptr = my_strtok(NULL, "\n")) == NULL) {
            nurly_log("warning: invalid configuration on line %2d: %s", cfg_file->current_line, cfg_line);
            continue;
        }
        if ((cfg_val = strdup(temp_ptr)) == NULL) {
            nurly_log("error: unable to allocate memory for configuration value");
            parse_error = TRUE;
            break;
        }

        strip(cfg_key);
        strip(cfg_val);

        if (!strcmp(cfg_key, "checks_url")) {
            if ((nurly_config->checks_url = strdup(cfg_val)) == NULL) {
                nurly_log("error: unable to allocate memory for checks_url value");
                parse_error = TRUE;
                break;
            } else {
                nurly_log("set checks url: %s", nurly_config->checks_url);
            }
        } else if (!strcmp(cfg_key, "health_url")) {
            if ((nurly_config->health_url = strdup(cfg_val)) == NULL) {
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
        } else if (!strcmp(cfg_key, "skip_host")) {
            if ((temp_reg = (regex_t*)malloc(sizeof(regex_t))) == NULL) {
                nurly_log("error: unable to allocate memory for skip_host pattern");
                parse_error = TRUE;
                break;
            }
            if ((parse_error = regcomp(temp_reg, cfg_val, REG_EXTENDED | REG_NOSUB | REG_ICASE)) != 0) {
                nurly_log_regerror(parse_error, temp_reg, cfg_val);
                parse_error = TRUE;
                break;
            }
            nurly_queue_put(&(nurly_config->skip_hosts), (void*)temp_reg);
            nurly_log("added skip_host pattern: %s", cfg_val);
        } else if (!strcmp(cfg_key, "skip_service")) {
            if ((temp_reg = (regex_t*)malloc(sizeof(regex_t))) == NULL) {
                nurly_log("error: unable to allocate memory for skip_service pattern");
                parse_error = TRUE;
                break;
            }
            if ((parse_error = regcomp(temp_reg, cfg_val, REG_EXTENDED | REG_NOSUB | REG_ICASE)) != 0) {
                nurly_log_regerror(parse_error, temp_reg, cfg_val);
                parse_error = TRUE;
                break;
            }
            nurly_queue_put(&(nurly_config->skip_services), (void*)temp_reg);
            nurly_log("added skip_service pattern: %s", cfg_val);
        } else {
            nurly_log("warning: unknown configuration key: %s", cfg_key);
        }
    }

    NURLY_FREE(cfg_key);
    NURLY_FREE(cfg_val);
    NURLY_FREE(cfg_line);
    mmap_fclose(cfg_file);

    if (nurly_config->checks_url == NULL) {
        nurly_log("error: no checks_url provided, nurly will be disabled.");
        parse_error = TRUE;
    }

    return parse_error == TRUE ? ERROR : OK;
}

void nurly_config_free(nurly_config_t* nurly_config) {
    NURLY_FREE(nurly_config->checks_url);
    NURLY_FREE(nurly_config->health_url);

    nurly_config->skip_hosts.purge = nurly_config_free_regex;
    nurly_queue_close(&(nurly_config->skip_hosts));

    nurly_config->skip_services.purge = nurly_config_free_regex;
    nurly_queue_close(&(nurly_config->skip_services));
}

int nurly_config_match(void* regex, void* text) {
    return regexec((regex_t*)regex, (char*)text, 0, NULL, 0) == 0;
}

void nurly_config_free_regex(void* data) {
    if (data) {
        regfree((regex_t*)data);
        NURLY_FREE(data);
    }
}
