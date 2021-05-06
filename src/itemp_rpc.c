#include <math.h>

#include <mgos-helpers/rpc.h>

#include <mgos-itemp.h>

#define SEND_CMD_FMT "{src:%H,cmd:%Q,arg:%f}"
static void itemp_send_cmd_handler(struct mg_rpc_request_info *ri, void *cb_arg,
                                   struct mg_rpc_frame_info *fi,
                                   struct mg_str args) {
  float arg = NAN;
  char *cmd = NULL;
  uint8_t *src = NULL;
  int src_sz = 0;
  json_scanf(args.p, args.len, ri->args_fmt, &src_sz, &src, &cmd, &arg);
  if (!src || !cmd) mg_rpc_errorf_gt(400, "src&cmd are required");
  if (src_sz != 3) mg_rpc_errorf_gt(400, "src must be 3 bytes");

  enum itemp_cmd itc;
  if (!strcasecmp(cmd, "ADJUST"))
    itc = ITCMD_ADJUST;
  else if (!strcasecmp(cmd, "COMFORT"))
    itc = ITCMD_COMFORT;
  else if (!strcasecmp(cmd, "SETBACK"))
    itc = ITCMD_SETBACK;
  else if (!strcasecmp(cmd, "WINDOW-OPEN"))
    itc = ITCMD_WINDOW_OPEN;
  else if (!strcasecmp(cmd, "WINDOW-SHUT"))
    itc = ITCMD_WINDOW_SHUT;
  else mg_rpc_errorf_gt(400, "unknown cmd %s", cmd);

  if (itc != ITCMD_ADJUST) {
    if (!isnan(arg)) mg_rpc_errorf_gt(400, "arg is redundant with cmd %s", cmd);
  } else {
    if (isnan(arg)) mg_rpc_errorf_gt(400, "arg is required with cmd %s", cmd);
    if (arg < -64 || arg > 63.5) mg_rpc_errorf_gt(400, "need -64..63.5 arg");
  }

  if (!mgos_itemp_send_cmd(src[0] << 16 | src[1] << 8 | src[2], itc, arg / 0.5))
    mg_rpc_errorf_gt(500, "%s() failed", "mgos_itemp_send_cmd");
  mg_rpc_send_responsef(ri, NULL);
err:
  if (cmd) free(cmd);
  if (src) free(src);
}

#define SETUP_RF_CMD_FMT "{reset:%B}"
static void itemp_setup_rf_handler(struct mg_rpc_request_info *ri, void *cb_arg,
                                   struct mg_rpc_frame_info *fi,
                                   struct mg_str args) {
  bool reset = false;
  json_scanf(args.p, args.len, ri->args_fmt, &reset);
  if (!mgos_itemp_setup_rf(reset))
    mg_rpc_errorf_ret(500, "%s() failed", "mgos_itemp_setup_rf");
  mg_rpc_send_responsef(ri, NULL);
}

static void itemp_status_handler(struct mg_rpc_request_info *ri, void *cb_arg,
                                 struct mg_rpc_frame_info *fi,
                                 struct mg_str args) {
  struct itemp_status st;
  if (!mgos_itemp_status(&st))
    mg_rpc_errorf_ret(500, "error getting iTemp status");
  mg_rpc_send_responsef(ri, "{busy:%B,ok:%B}", st.busy, st.ok);
}

bool mgos_rpc_service_itemp_init() {
  struct mg_rpc *rpc = mgos_rpc_get_global();
  if (!rpc || !mgos_sys_config_get_itemp_rpc_enable()) return true;
  mg_rpc_add_handler(rpc, "iTemp.SendCmd", SEND_CMD_FMT, itemp_send_cmd_handler,
                     NULL);
  mg_rpc_add_handler(rpc, "iTemp.SetupRf", SETUP_RF_CMD_FMT,
                     itemp_setup_rf_handler, NULL);
  mg_rpc_add_handler(rpc, "iTemp.Status", "", itemp_status_handler, NULL);
  return true;
}
