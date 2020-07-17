/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * DOC: wlan_cm_roam_api.c
 *
 * Implementation for the Common Roaming interfaces.
 */

#include "wlan_cm_roam_api.h"
#include "wlan_vdev_mlme_api.h"
#include "wlan_mlme_main.h"
#include "wlan_policy_mgr_api.h"
#include <wmi_unified_priv.h>
#include "../../core/src/wlan_cm_roam_offload.h"

#ifdef ROAM_OFFLOAD_V1
#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
QDF_STATUS
wlan_cm_enable_roaming_on_connected_sta(struct wlan_objmgr_pdev *pdev,
					uint8_t vdev_id)
{
	uint32_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t sta_vdev_id = WLAN_INVALID_VDEV_ID;
	uint32_t count;
	uint32_t idx;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	sta_vdev_id = policy_mgr_get_roam_enabled_sta_session_id(psoc, vdev_id);
	if (sta_vdev_id != WLAN_UMAC_VDEV_ID_MAX)
		return QDF_STATUS_E_FAILURE;

	count = policy_mgr_get_mode_specific_conn_info(psoc,
						       op_ch_freq_list,
						       vdev_id_list,
						       PM_STA_MODE);

	if (!count)
		return QDF_STATUS_E_FAILURE;

	/*
	 * Loop through all connected STA vdevs and roaming will be enabled
	 * on the STA that has a different vdev id from the one passed as
	 * input and has non zero roam trigger value.
	 */
	for (idx = 0; idx < count; idx++) {
		if (vdev_id_list[idx] != vdev_id &&
		    mlme_get_roam_trigger_bitmap(psoc, vdev_id_list[idx])) {
			sta_vdev_id = vdev_id_list[idx];
			break;
		}
	}

	if (sta_vdev_id == WLAN_INVALID_VDEV_ID)
		return QDF_STATUS_E_FAILURE;

	mlme_debug("ROAM: Enabling roaming on vdev[%d]", sta_vdev_id);

	return cm_roam_state_change(pdev,
				    sta_vdev_id,
				    WLAN_ROAM_RSO_ENABLED,
				    REASON_CTX_INIT);
}

QDF_STATUS wlan_cm_start_roaming(struct wlan_objmgr_pdev *pdev,
				 uint8_t vdev_id, uint8_t reason)
{
	return cm_roam_state_change(pdev, vdev_id,
				    WLAN_ROAM_RSO_ENABLED, reason);
}

QDF_STATUS wlan_cm_roam_send_rso_cmd(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id,
				     uint8_t rso_command,
				     uint8_t reason)
{
	return cm_roam_send_rso_cmd(psoc, vdev_id, rso_command, reason);
}
#endif
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS
wlan_cm_roam_extract_btm_response(wmi_unified_t wmi, void *evt_buf,
				  struct roam_btm_response_data *dst,
				  uint8_t idx)
{
	if (wmi->ops->extract_roam_btm_response_stats)
		return wmi->ops->extract_roam_btm_response_stats(wmi, evt_buf,
								 dst, idx);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wlan_cm_roam_extract_roam_initial_info(wmi_unified_t wmi, void *evt_buf,
				       struct roam_initial_data *dst,
				       uint8_t idx)
{
	if (wmi->ops->extract_roam_initial_info)
		return wmi->ops->extract_roam_initial_info(wmi, evt_buf,
							   dst, idx);

	return QDF_STATUS_E_FAILURE;
}

void wlan_cm_roam_activate_pcl_per_vdev(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id, bool pcl_per_vdev)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	/* value - true (vdev pcl) false - pdev pcl */
	mlme_priv->cm_roam.pcl_vdev_cmd_active = pcl_per_vdev;
	mlme_legacy_debug("CM_ROAM: vdev[%d] SET PCL cmd level - [%s]", vdev_id,
			  pcl_per_vdev ? "VDEV" : "PDEV");
}

bool wlan_cm_roam_is_pcl_per_vdev_active(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return false;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return false;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return mlme_priv->cm_roam.pcl_vdev_cmd_active;
}

bool
wlan_cm_dual_sta_is_freq_allowed(struct wlan_objmgr_psoc *psoc,
				 uint32_t freq,
				 enum QDF_OPMODE opmode)
{
	uint32_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	enum reg_wifi_band band;
	uint32_t count, connected_sta_freq;

	/*
	 * Check if already there is 1 STA connected. If this API is
	 * called for 2nd STA and if dual sta roaming is enabled, then
	 * don't allow the intra band frequencies of the 1st sta for
	 * connection on 2nd STA.
	 */
	count = policy_mgr_get_mode_specific_conn_info(psoc, op_ch_freq_list,
						       vdev_id_list,
						       PM_STA_MODE);
	if (!count || !wlan_mlme_get_dual_sta_roaming_enabled(psoc) ||
	    opmode != QDF_STA_MODE)
		return true;

	connected_sta_freq = op_ch_freq_list[0];
	band = wlan_reg_freq_to_band(connected_sta_freq);
	if ((band == REG_BAND_2G && WLAN_REG_IS_24GHZ_CH_FREQ(freq)) ||
	    (band == REG_BAND_5G && !WLAN_REG_IS_24GHZ_CH_FREQ(freq)))
		return false;

	return true;
}

void
wlan_cm_dual_sta_roam_update_connect_channels(struct wlan_objmgr_psoc *psoc,
					      struct scan_filter *filter)
{
	uint32_t i, num_channels = 0;
	uint32_t *channel_list;
	bool is_ch_allowed;
	QDF_STATUS status;

	if (!wlan_mlme_get_dual_sta_roaming_enabled(psoc))
		return;

	channel_list = qdf_mem_malloc(NUM_CHANNELS * sizeof(uint32_t));
	if (!channel_list)
		return;

	/*
	 * Get Reg domain valid channels and update to the scan filter
	 * if already 1st sta is in connected state. Don't allow channels
	 * on which the 1st STA is connected.
	 */
	status = policy_mgr_get_valid_chans(psoc, channel_list,
					    &num_channels);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_legacy_err("Error in getting valid channels");
		qdf_mem_free(channel_list);
		return;
	}

	filter->num_of_channels = 0;
	for (i = 0; i < num_channels; i++) {
		is_ch_allowed =
			wlan_cm_dual_sta_is_freq_allowed(psoc, channel_list[i],
							 QDF_STA_MODE);
		if (!is_ch_allowed)
			continue;

		filter->chan_freq_list[filter->num_of_channels] =
					channel_list[i];
		filter->num_of_channels++;
	}
	qdf_mem_free(channel_list);
}

void
wlan_cm_roam_disable_vendor_btm(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return;
	}

	/* Set default value of reason code */
	mlme_priv->cm_roam.vendor_btm_param.user_roam_reason =
						DISABLE_VENDOR_BTM_CONFIG;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
}

void
wlan_cm_roam_set_vendor_btm_params(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id,
				   struct wlan_cm_roam_vendor_btm_params
									*param)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return;
	}

	qdf_mem_copy(&mlme_priv->cm_roam.vendor_btm_param, param,
		     sizeof(struct wlan_cm_roam_vendor_btm_params));
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
}

void
wlan_cm_roam_get_vendor_btm_params(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id,
				   struct wlan_cm_roam_vendor_btm_params *param)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return;
	}

	qdf_mem_copy(param, &mlme_priv->cm_roam.vendor_btm_param,
		     sizeof(struct wlan_cm_roam_vendor_btm_params));

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
}
#endif
