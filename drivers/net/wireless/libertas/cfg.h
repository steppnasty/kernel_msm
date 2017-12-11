#ifndef __LBS_CFG80211_H__
#define __LBS_CFG80211_H__

struct device;
struct lbs_private;

struct wireless_dev *lbs_cfg_alloc(struct device *dev);
int lbs_cfg_register(struct lbs_private *priv);
void lbs_cfg_free(struct lbs_private *priv);

/* All of those are TODOs: */
#define lbs_cmd_802_11_rssi(priv, cmdptr) (0)
#define lbs_ret_802_11_rssi(priv, resp) (0)
#define lbs_cmd_bcn_ctrl(priv, cmdptr, cmd_action) (0)
#define lbs_ret_802_11_bcn_ctrl(priv, resp) (0)

void lbs_send_disconnect_notification(struct lbs_private *priv);
void lbs_send_mic_failureevent(struct lbs_private *priv, u32 event);

void lbs_scan_deinit(struct lbs_private *priv);

#endif
