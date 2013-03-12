#define NURLY_CONFIG_INITIALIZER { NULL, NULL, 2, 20 }

typedef struct nurly_config {
    char* checks_url;
    char* health_url;
    int   health_interval;
    int   worker_threads;
} nurly_config_t;

int  nurly_config_read(char*, nurly_config_t*);
void nurly_config_free(nurly_config_t*);
