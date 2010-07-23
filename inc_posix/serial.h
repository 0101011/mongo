/* DO NOT EDIT: automatically built by dist/serial.py. */

typedef struct {
	WT_PAGE * page;
	u_int16_t write_gen;
	int slot;
	WT_COL_EXPAND ** new_expcol;
	WT_COL_EXPAND * exp;
} __wt_bt_rcc_expand_args;
#define	 __wt_bt_rcc_expand_serial(toc, _page, _write_gen, _slot, _new_expcol, _exp, ret) do {\
	__wt_bt_rcc_expand_args _args;\
	_args.page = _page;\
	_args.write_gen = _write_gen;\
	_args.slot = _slot;\
	_args.new_expcol = _new_expcol;\
	_args.exp = _exp;\
	(ret) = __wt_toc_serialize_func(\
	    toc, WT_WORKQ_SPIN, __wt_bt_rcc_expand_serial_func, &_args);\
} while (0)
#define	__wt_bt_rcc_expand_unpack(toc, _page, _write_gen, _slot, _new_expcol, _exp) do {\
	_page = ((__wt_bt_rcc_expand_args *)(toc)->wq_args)->page;\
	_write_gen = ((__wt_bt_rcc_expand_args *)(toc)->wq_args)->write_gen;\
	_slot = ((__wt_bt_rcc_expand_args *)(toc)->wq_args)->slot;\
	_new_expcol = ((__wt_bt_rcc_expand_args *)(toc)->wq_args)->new_expcol;\
	_exp = ((__wt_bt_rcc_expand_args *)(toc)->wq_args)->exp;\
} while (0)

typedef struct {
	WT_PAGE * page;
	u_int16_t write_gen;
	WT_COL_EXPAND * exp;
	WT_REPL * repl;
} __wt_bt_rcc_expand_repl_args;
#define	 __wt_bt_rcc_expand_repl_serial(toc, _page, _write_gen, _exp, _repl, ret) do {\
	__wt_bt_rcc_expand_repl_args _args;\
	_args.page = _page;\
	_args.write_gen = _write_gen;\
	_args.exp = _exp;\
	_args.repl = _repl;\
	(ret) = __wt_toc_serialize_func(\
	    toc, WT_WORKQ_SPIN, __wt_bt_rcc_expand_repl_serial_func, &_args);\
} while (0)
#define	__wt_bt_rcc_expand_repl_unpack(toc, _page, _write_gen, _exp, _repl) do {\
	_page = ((__wt_bt_rcc_expand_repl_args *)(toc)->wq_args)->page;\
	_write_gen = ((__wt_bt_rcc_expand_repl_args *)(toc)->wq_args)->write_gen;\
	_exp = ((__wt_bt_rcc_expand_repl_args *)(toc)->wq_args)->exp;\
	_repl = ((__wt_bt_rcc_expand_repl_args *)(toc)->wq_args)->repl;\
} while (0)

typedef struct {
	WT_PAGE * page;
	u_int16_t write_gen;
	int slot;
	WT_REPL ** new_repl;
	WT_REPL * repl;
} __wt_bt_update_args;
#define	 __wt_bt_update_serial(toc, _page, _write_gen, _slot, _new_repl, _repl, ret) do {\
	__wt_bt_update_args _args;\
	_args.page = _page;\
	_args.write_gen = _write_gen;\
	_args.slot = _slot;\
	_args.new_repl = _new_repl;\
	_args.repl = _repl;\
	(ret) = __wt_toc_serialize_func(\
	    toc, WT_WORKQ_SPIN, __wt_bt_update_serial_func, &_args);\
} while (0)
#define	__wt_bt_update_unpack(toc, _page, _write_gen, _slot, _new_repl, _repl) do {\
	_page = ((__wt_bt_update_args *)(toc)->wq_args)->page;\
	_write_gen = ((__wt_bt_update_args *)(toc)->wq_args)->write_gen;\
	_slot = ((__wt_bt_update_args *)(toc)->wq_args)->slot;\
	_new_repl = ((__wt_bt_update_args *)(toc)->wq_args)->new_repl;\
	_repl = ((__wt_bt_update_args *)(toc)->wq_args)->repl;\
} while (0)

typedef struct {
	u_int32_t addr;
	u_int32_t size;
	WT_PAGE ** pagep;
} __wt_cache_in_args;
#define	 __wt_cache_in_serial(toc, _addr, _size, _pagep, ret) do {\
	__wt_cache_in_args _args;\
	_args.addr = _addr;\
	_args.size = _size;\
	_args.pagep = _pagep;\
	(ret) = __wt_toc_serialize_func(\
	    toc, WT_WORKQ_READ, __wt_cache_in_serial_func, &_args);\
} while (0)
#define	__wt_cache_in_unpack(toc, _addr, _size, _pagep) do {\
	_addr = ((__wt_cache_in_args *)(toc)->wq_args)->addr;\
	_size = ((__wt_cache_in_args *)(toc)->wq_args)->size;\
	_pagep = ((__wt_cache_in_args *)(toc)->wq_args)->pagep;\
} while (0)
