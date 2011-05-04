/* DO NOT EDIT: automatically built by dist/s_prototypes. */

#ifdef __GNUC__
#define	WT_GCC_ATTRIBUTE(x)	__attribute__(x)
#else
#define	WT_GCC_ATTRIBUTE(x)
#endif

void __wt_methods_btree_config_default(BTREE *btree);
void __wt_methods_btree_lockout(BTREE *btree);
void __wt_methods_btree_init_transition(BTREE *btree);
void __wt_methods_btree_open_transition(BTREE *btree);
void __wt_methods_connection_config_default(CONNECTION *connection);
void __wt_methods_connection_lockout(CONNECTION *connection);
void __wt_methods_connection_open_transition(CONNECTION *connection);
void __wt_methods_connection_init_transition(CONNECTION *connection);
void __wt_methods_session_lockout(SESSION *session);
void __wt_methods_session_init_transition(SESSION *session);
int __wt_config_initn(WT_CONFIG *conf, const char *str, size_t len);
int __wt_config_init(WT_CONFIG *conf, const char *str);
int __wt_config_next(WT_CONFIG *conf,
    WT_CONFIG_ITEM *key,
    WT_CONFIG_ITEM *value);
int __wt_config_getraw( WT_CONFIG *cparser,
    WT_CONFIG_ITEM *key,
    WT_CONFIG_ITEM *value);
int __wt_config_get(const char **cfg,
    WT_CONFIG_ITEM *key,
    WT_CONFIG_ITEM *value);
int __wt_config_gets(const char **cfg, const char *key, WT_CONFIG_ITEM *value);
 int __wt_config_getone(const char *cfg,
    WT_CONFIG_ITEM *key,
    WT_CONFIG_ITEM *value);
 int __wt_config_getones(const char *cfg,
    const char *key,
    WT_CONFIG_ITEM *value);
int __wt_config_checklist(SESSION *session,
    const char **defaults,
    const char *config);
int __wt_config_check(SESSION *session,
    const char *defaults,
    const char *config);
const char *__wt_config_def_add_collator;
const char *__wt_config_def_add_cursor_type;
const char *__wt_config_def_add_extractor;
const char *__wt_config_def_begin_transaction;
const char *__wt_config_def_checkpoint;
const char *__wt_config_def_commit_transaction;
const char *__wt_config_def_connection_close;
const char *__wt_config_def_create_table;
const char *__wt_config_def_cursor_close;
const char *__wt_config_def_load_extension;
const char *__wt_config_def_open_cursor;
const char *__wt_config_def_rename_table;
const char *__wt_config_def_rollback_transaction;
const char *__wt_config_def_session_close;
const char *__wt_config_def_truncate_table;
const char *__wt_config_def_verify_table;
const char *__wt_config_def_wiredtiger_open;
int __wt_session_add_btree(SESSION *session,
    BTREE *btree,
    const char *key_format,
    const char *value_format);
int __wt_cursor_open(SESSION *session,
    const char *uri,
    const char *config,
    WT_CURSOR **cursorp);
int __wt_curbulk_init(CURSOR_BULK *cbulk);
int __wt_curconfig_open(SESSION *session,
    const char *uri,
    const char *config,
    WT_CURSOR **cursorp);
void __wt_curdump_init(WT_CURSOR *cursor);
int __wt_cursor_close(WT_CURSOR *cursor, const char *config);
void __wt_cursor_init(WT_CURSOR *cursor, const char *config);
int __wt_block_alloc(SESSION *session, uint32_t *addrp, uint32_t size);
int __wt_block_free(SESSION *session, uint32_t addr, uint32_t size);
int __wt_block_read(SESSION *session);
int __wt_block_write(SESSION *session);
void __wt_block_discard(SESSION *session);
void __wt_block_dump(SESSION *session);
int __wt_btree_bulk_load(SESSION *session,
    int (*cb)(BTREE *,
    WT_ITEM **,
    WT_ITEM **));
int __wt_bulk_init(CURSOR_BULK *cbulk);
int __wt_bulk_var_insert(CURSOR_BULK *cbulk);
int __wt_bulk_end(CURSOR_BULK *cbulk);
int __wt_item_build_key( SESSION *session,
    WT_BUF *key,
    WT_CELL *cell,
    WT_OVFL *ovfl);
int __wt_item_build_value( SESSION *session,
    WT_BUF *value,
    WT_CELL *cell,
    WT_OVFL *ovfl);
int __wt_cache_create(CONNECTION *conn);
void __wt_cache_stats_update(CONNECTION *conn);
void __wt_cache_destroy(CONNECTION *conn);
int __wt_bt_close(SESSION *session);
int __wt_bt_lex_compare( BTREE *btree,
    const WT_ITEM *user_item,
    const WT_ITEM *tree_item);
int __wt_bt_int_compare( BTREE *btree,
    const WT_ITEM *user_item,
    const WT_ITEM *tree_item);
int __wt_btcur_first(CURSOR_BTREE *cbt);
int __wt_btcur_next(CURSOR_BTREE *cbt);
int __wt_btcur_prev(CURSOR_BTREE *cbt);
int __wt_btcur_search_near(CURSOR_BTREE *cbt, int *exact);
int __wt_btcur_insert(CURSOR_BTREE *cbt);
int __wt_btcur_update(CURSOR_BTREE *cbt);
int __wt_btcur_remove(CURSOR_BTREE *cbt);
int __wt_btcur_close(CURSOR_BTREE *cbt, const char *config);
int __wt_debug_dump(SESSION *session, const char *ofile, FILE *fp);
int __wt_debug_disk( SESSION *session,
    WT_PAGE_DISK *dsk,
    const char *ofile,
    FILE *fp);
int __wt_debug_tree_all( SESSION *session,
    WT_PAGE *page,
    const char *ofile,
    FILE *fp);
int __wt_debug_tree(SESSION *session,
    WT_PAGE *page,
    const char *ofile,
    FILE *fp);
int __wt_debug_page(SESSION *session,
    WT_PAGE *page,
    const char *ofile,
    FILE *fp);
void __wt_debug_item(const char *tag, void *arg_item, FILE *fp);
int __wt_desc_stat(SESSION *session);
int __wt_desc_read(SESSION *session);
int __wt_desc_write(SESSION *session);
void __wt_page_free(SESSION *session, WT_PAGE *page);
int __wt_btree_dump(SESSION *session, FILE *stream, uint32_t flags);
void __wt_print_byte_string(const uint8_t *data, uint32_t size, FILE *stream);
void __wt_workq_evict_server(CONNECTION *conn, int force);
int __wt_evict_file_serial_func(SESSION *session);
void *__wt_cache_evict_server(void *arg);
void __wt_workq_evict_server_exit(CONNECTION *conn);
void __wt_evict_dump(SESSION *session);
const char *__wt_page_type_string(u_int type);
const char *__wt_cell_type_string(WT_CELL *cell);
int __wt_bt_open(SESSION *session, int ok_create);
int __wt_root_pin(SESSION *session);
int __wt_ovfl_in(SESSION *session, WT_OVFL *ovfl, WT_BUF *store);
int
__wt_page_in_func(SESSION *session, WT_PAGE *parent, WT_REF *ref, int dsk_verify
#ifdef HAVE_DIAGNOSTIC
 , const char *file, int line
#endif
 );
int __wt_page_inmem(SESSION *session, WT_PAGE *page);
int __wt_cell_process(SESSION *session, const WT_CELL *cell, WT_BUF *retbuf);
void __wt_workq_read_server(CONNECTION *conn, int force);
int __wt_cache_read_serial_func(SESSION *session);
void *__wt_cache_read_server(void *arg);
void __wt_workq_read_server_exit(CONNECTION *conn);
void __wt_rec_destroy(SESSION *session);
int __wt_page_reconcile( SESSION *session,
    WT_PAGE *page,
    uint32_t slvg_skip,
    int caller);
int __wt_return_data(SESSION *session,
    WT_ITEM *key,
    WT_ITEM *value,
    int key_return);
int __wt_disk_read( SESSION *session,
    WT_PAGE_DISK *dsk,
    uint32_t addr,
    uint32_t size);
int __wt_disk_write( SESSION *session,
    WT_PAGE_DISK *dsk,
    uint32_t addr,
    uint32_t size);
int __wt_btree_salvage(SESSION *session);
void __wt_trk_dump(const char *l, void *ss_arg);
int __wt_page_stat(SESSION *session, WT_PAGE *page, void *arg);
int __wt_bt_sync(SESSION *session);
int __wt_btree_verify(SESSION *session);
int __wt_verify(SESSION *session, FILE *stream);
int __wt_verify_dsk_page( SESSION *session,
    WT_PAGE_DISK *dsk,
    uint32_t addr,
    uint32_t size);
int __wt_verify_dsk_chunk( SESSION *session,
    WT_PAGE_DISK *dsk,
    uint32_t addr,
    uint32_t size);
int __wt_tree_walk(SESSION *session,
    WT_PAGE *page,
    uint32_t flags,
    int (*work)(SESSION *,
    WT_PAGE *,
    void *),
    void *arg);
int __wt_walk_begin(SESSION *session, WT_REF *ref, WT_WALK *walk);
void __wt_walk_end(SESSION *session, WT_WALK *walk);
int __wt_walk_next(SESSION *session,
    WT_WALK *walk,
    uint32_t flags,
    WT_REF **refp);
int __wt_btree_col_get(SESSION *session, uint64_t recno, WT_ITEM *value);
int __wt_btree_col_del(SESSION *session, uint64_t recno);
int __wt_btree_col_put(SESSION *session, uint64_t recno, WT_ITEM *value);
int __wt_col_search(SESSION *session, uint64_t recno, uint32_t flags);
int __wt_btree_row_get(SESSION *session, WT_ITEM *key, WT_ITEM *value);
int __wt_key_build(SESSION *session,
    WT_PAGE *page,
    void *rip_arg,
    WT_BUF *store);
int __wt_key_build_serial_func(SESSION *session);
int __wt_btree_row_del(SESSION *session, WT_ITEM *key);
int __wt_btree_row_put(SESSION *session, WT_ITEM *key, WT_ITEM *value);
int __wt_insert_serial_func(SESSION *session);
int __wt_update_alloc(SESSION *session, WT_ITEM *value, WT_UPDATE **updp);
int __wt_update_serial_func(SESSION *session);
int __wt_row_search(SESSION *session, WT_ITEM *key, uint32_t flags);
int __wt_btree_btree_compare_int_set_verify(BTREE *btree,
    int btree_compare_int);
int __wt_btree_column_set_verify( BTREE *btree,
    uint32_t fixed_len,
    const char *dictionary,
    uint32_t flags);
int __wt_connection_btree(CONNECTION *conn, BTREE **btreep);
int __wt_btree_destroy(BTREE *btree);
int __wt_btree_lockout_err(BTREE *btree);
int __wt_btree_lockout_open(BTREE *btree);
int __wt_btree_huffman_set(BTREE *btree,
    uint8_t const *huffman_table,
    u_int huffman_table_size,
    uint32_t flags);
int __wt_btree_open(SESSION *session,
    const char *name,
    mode_t mode,
    uint32_t flags);
int __wt_btree_close(SESSION *session, uint32_t flags);
int __wt_btree_stat_print(SESSION *session, FILE *stream);
int __wt_btree_stat_clear(BTREE *btree);
int __wt_btree_sync(SESSION *session, uint32_t flags);
int __wt_connection_cache_size_set_verify(CONNECTION *conn,
    uint32_t cache_size);
int __wt_connection_cache_hash_size_set_verify(CONNECTION *conn,
    uint32_t hash_size);
int __wt_connection_hazard_size_set_verify(CONNECTION *conn,
    uint32_t hazard_size);
int __wt_connection_session_size_set_verify(CONNECTION *conn,
    uint32_t toc_size);
int __wt_connection_verbose_set_verify(CONNECTION *conn, uint32_t verbose);
int __wt_library_init(void);
int __wt_breakpoint(void);
void __wt_attach(SESSION *session);
int __wt_connection_config(CONNECTION *conn);
int __wt_connection_destroy(CONNECTION *conn);
void __wt_mb_init(SESSION *session, WT_MBUF *mbp);
void __wt_mb_discard(WT_MBUF *mbp);
void __wt_mb_add(WT_MBUF *mbp, const char *fmt, ...);
void __wt_mb_write(WT_MBUF *mbp);
int __wt_connection_open(CONNECTION *conn, const char *home, mode_t mode);
int __wt_connection_close(CONNECTION *conn);
int __wt_connection_session(CONNECTION *conn, SESSION **sessionp);
int __wt_session_close(SESSION *session);
int __wt_session_api_set(CONNECTION *conn,
    const char *name,
    BTREE *btree,
    SESSION **sessionp,
    int *islocal);
int __wt_session_api_clr(SESSION *session, const char *name, int islocal);
void __wt_session_dump(SESSION *session);
int __wt_connection_stat_print(CONNECTION *conn, FILE *stream);
int __wt_connection_stat_clear(CONNECTION *conn);
void __wt_stat_print(WT_STATS *s, FILE *stream);
int __wt_connection_sync(CONNECTION *conn);
void *__wt_workq_srvr(void *arg);
int __wt_log_put(SESSION *session, WT_LOGREC_DESC *recdesc, ...);
int __wt_log_printf(SESSION *session,
    const char *fmt,
    ...) WT_GCC_ATTRIBUTE ((format (printf,
    2,
    3)));
WT_LOGREC_DESC __wt_logdesc_debug;
void __wt_abort(SESSION *session);
int __wt_calloc(SESSION *session, size_t number, size_t size, void *retp);
int __wt_realloc(SESSION *session,
    uint32_t *bytes_allocated_ret,
    size_t bytes_to_allocate,
    void *retp);
int __wt_strdup(SESSION *session, const char *str, void *retp);
void __wt_free_int(SESSION *session, void *p_arg);
int __wt_filesize(SESSION *session, WT_FH *fh, off_t *sizep);
int __wt_fsync(SESSION *session, WT_FH *fh);
int __wt_ftruncate(SESSION *session, WT_FH *fh, off_t len);
int __wt_mtx_alloc(SESSION *session,
    const char *name,
    int is_locked,
    WT_MTX **mtxp);
void __wt_lock(SESSION *session, WT_MTX *mtx);
void __wt_unlock(SESSION *session, WT_MTX *mtx);
int __wt_mtx_destroy(SESSION *session, WT_MTX *mtx);
int __wt_open( SESSION *session,
    const char *name,
    mode_t mode,
    int ok_create,
    WT_FH **fhp);
int __wt_close(SESSION *session, WT_FH *fh);
int __wt_read(SESSION *session,
    WT_FH *fh,
    off_t offset,
    uint32_t bytes,
    void *buf);
int __wt_write(SESSION *session,
    WT_FH *fh,
    off_t offset,
    uint32_t bytes,
    void *buf);
void __wt_sleep(long seconds, long micro_seconds);
int __wt_thread_create(pthread_t *tidret, void *(*func)(void *), void *arg);
void __wt_thread_join(pthread_t tid);
void __wt_yield(void);
uint32_t __wt_cksum(const void *chunk, size_t len);
void __wt_msgv(SESSION *session,
    const char *prefix1,
    const char *prefix2,
    const char *fmt,
    va_list ap);
void __wt_msg(SESSION *session, const char *fmt, ...);
void __wt_assert( SESSION *session,
    const char *check,
    const char *file_name,
    int line_number);
int __wt_api_args(SESSION *session, const char *name);
int __wt_api_arg_min(SESSION *session,
    const char *name,
    const char *arg_name,
    uint32_t v,
    uint32_t min);
int __wt_api_arg_max(SESSION *session,
    const char *name,
    const char *arg_name,
    uint32_t v,
    uint32_t max);
int __wt_file_method_type(SESSION *session, const char *name, int column_err);
int __wt_file_wrong_fixed_size(SESSION *session,
    uint32_t len,
    uint32_t config_len);
int __wt_file_readonly(SESSION *session, const char *name);
int __wt_file_format(SESSION *session);
int __wt_file_item_too_big(SESSION *session);
int __wt_session_lockout(SESSION *session);
int __wt_btree_lockout(BTREE *btree);
int __wt_connection_lockout(CONNECTION *conn);
void __wt_errv(SESSION *session,
    int error,
    const char *prefix1,
    const char *prefix2,
    const char *fmt,
    va_list ap);
void __wt_err(SESSION *session, int error, const char *fmt, ...);
void __wt_errx(SESSION *session, const char *fmt, ...);
int
__wt_hazard_set(SESSION *session, WT_REF *ref
#ifdef HAVE_DIAGNOSTIC
 , const char *file, int line
#endif
 );
void __wt_hazard_clear(SESSION *session, WT_PAGE *page);
void __wt_hazard_empty(SESSION *session, const char *name);
void __wt_hazard_validate(SESSION *session, WT_PAGE *page);
int __wt_huffman_open(SESSION *session,
    uint8_t const *byte_frequency_array,
    u_int nbytes,
    void *retp);
void __wt_huffman_close(SESSION *session, void *huffman_arg);
int __wt_print_huffman_code(SESSION *session,
    void *huffman_arg,
    uint16_t symbol);
int __wt_huffman_encode(void *huffman_arg,
    const uint8_t *from,
    uint32_t from_len,
    WT_BUF *to_buf);
int __wt_huffman_decode(void *huffman_arg,
    const uint8_t *from,
    uint32_t from_len,
    WT_BUF *to_buf);
uint32_t __wt_nlpo2_round(uint32_t v);
uint32_t __wt_nlpo2(uint32_t v);
int __wt_ispo2(uint32_t v);
uint32_t __wt_prime(uint32_t n);
int __wt_buf_setsize(SESSION *session, WT_BUF *buf, size_t sz);
void __wt_buf_clear(WT_BUF *buf);
void __wt_buf_free(SESSION *session, WT_BUF *buf);
int __wt_scr_alloc(SESSION *session, uint32_t size, WT_BUF **scratchp);
void __wt_scr_release(WT_BUF **bufp);
void __wt_scr_free(SESSION *session);
int __wt_session_serialize_func(SESSION *session,
    wq_state_t op,
    int spin,
    int (*func)(SESSION *),
    void *args);
void __wt_session_serialize_wrapup(SESSION *session, WT_PAGE *page, int ret);
int __wt_sb_alloc(SESSION *session,
    size_t size,
    void *retp,
    SESSION_BUFFER **sbp);
void __wt_sb_free(SESSION *session, SESSION_BUFFER *sb);
void __wt_sb_decrement(SESSION *session, SESSION_BUFFER *sb);
int __wt_stat_alloc_btree_stats(SESSION *session, WT_BTREE_STATS **statsp);
void __wt_stat_clear_btree_stats(WT_BTREE_STATS *stats);
void __wt_stat_print_btree_stats(WT_BTREE_STATS *stats, FILE *stream);
int __wt_stat_alloc_btree_file_stats(SESSION *session,
    WT_BTREE_FILE_STATS **statsp);
void __wt_stat_clear_btree_file_stats(WT_BTREE_FILE_STATS *stats);
void __wt_stat_print_btree_file_stats(WT_BTREE_FILE_STATS *stats, FILE *stream);
int __wt_stat_alloc_cache_stats(SESSION *session, WT_CACHE_STATS **statsp);
void __wt_stat_clear_cache_stats(WT_CACHE_STATS *stats);
void __wt_stat_print_cache_stats(WT_CACHE_STATS *stats, FILE *stream);
int __wt_stat_alloc_conn_stats(SESSION *session, WT_CONN_STATS **statsp);
void __wt_stat_clear_conn_stats(WT_CONN_STATS *stats);
void __wt_stat_print_conn_stats(WT_CONN_STATS *stats, FILE *stream);
int __wt_stat_alloc_file_stats(SESSION *session, WT_FILE_STATS **statsp);
void __wt_stat_clear_file_stats(WT_FILE_STATS *stats);
void __wt_stat_print_file_stats(WT_FILE_STATS *stats, FILE *stream);

#ifdef __GNUC__
#undef	WT_GCC_ATTRIBUTE
#define	WT_GCC_ATTRIBUTE(x)
#endif
