float get_stat(const char *stat);
int set_stat(const char *stat, const float new_value);
int increase_stat(const char *stat);
int decrease_stat(const char *stat);
int reset_stat(const char* stat);
int remove_stat(const char *stat);
float calculate_ewma(const float old_ewma, const float new_value, const float factor);