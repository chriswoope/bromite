/*
    This file is part of Bromite.

    Bromite is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Bromite is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Bromite. If not, see <https://www.gnu.org/licenses/>.
*/

package org.chromium.components.user_scripts;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;
import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItemWithEndIcon;

import android.content.Intent;
import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;
import android.widget.Toast;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.ActivityWindowAndroid;

import org.chromium.base.ApplicationStatus;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import org.chromium.components.user_scripts.ScriptListBaseAdapter;
import org.chromium.components.user_scripts.FragmentWindowAndroid;

public class ScriptListPreference extends Preference {
    private static class ScriptListAdapter
            extends ScriptListBaseAdapter {
        private final Context mContext;

        ScriptListAdapter(Context context) {
            super(context);
            mContext = context;
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {
            super.onBindViewHolder(holder, position);

            final ScriptInfo info = getItemByPosition(position);

            showDragIndicatorInRow((ScriptInfoRowViewHolder) holder);
            ModelList menuItems = new ModelList();

            menuItems.add(buildMenuListItem(R.string.remove, 0, 0, true));
            menuItems.add(buildMenuListItem(R.string.scripts_menu_enable, 0, 0, info.Enabled == false));
            menuItems.add(buildMenuListItem(R.string.scripts_menu_disable, 0, 0, info.Enabled == true));

            ListMenu.Delegate delegate = (model) -> {
                int textId = model.get(ListMenuItemProperties.TITLE_ID);
                if (textId == R.string.remove) {
                    UserScriptsBridge.RemoveScript(info.Key);
                } else if (textId == R.string.scripts_menu_enable) {
                    UserScriptsBridge.SetScriptEnabled(info.Key, true);
                } else if (textId == R.string.scripts_menu_disable) {
                    UserScriptsBridge.SetScriptEnabled(info.Key, false);
                }
            };
            ((ScriptInfoRowViewHolder) holder)
                    .setMenuButtonDelegate(() -> new BasicListMenu(mContext, menuItems, delegate));
        }

        // @Override
        public void onDataUpdated() {
            if (mDragStateDelegate.getDragActive()) {
                enableDrag();
            } else {
                disableDrag();
            }
            setDisplayedScriptInfo(UserScriptsBridge.getUserScriptItems());
        }
    }

    private TextView mAddButton;
    private RecyclerView mRecyclerView;
    private ScriptListAdapter mAdapter;
    private FragmentWindowAndroid mwindowAndroid;

    public ScriptListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAdapter = new ScriptListAdapter(context);
    }

    public void setWindowAndroid(FragmentWindowAndroid windowAndroid) {
        mwindowAndroid = windowAndroid;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mAddButton = (TextView) holder.findViewById(R.id.add_script);
        mAddButton.setCompoundDrawablesRelativeWithIntrinsicBounds(
                TintedDrawable.constructTintedDrawable(
                        getContext(), R.drawable.plus, R.color.default_control_color_active),
                null, null, null);
        mAddButton.setOnClickListener(view -> {
            UserScriptsBridge.SelectAndAddScriptFromFile(mwindowAndroid);
        });

        mRecyclerView = (RecyclerView) holder.findViewById(R.id.script_list);
        LinearLayoutManager layoutManager = new LinearLayoutManager(getContext());
        mRecyclerView.setLayoutManager(layoutManager);
        mRecyclerView.addItemDecoration(
                new DividerItemDecoration(getContext(), layoutManager.getOrientation()));

        UserScriptsBridge.RegisterLoadCallback(this);

        // We do not want the RecyclerView to be announced by screen readers every time
        // the view is bound.
        if (mRecyclerView.getAdapter() != mAdapter) {
            mRecyclerView.setAdapter(mAdapter);
            // Initialize script list.
            mAdapter.onDataUpdated();
        }
    }

    public void NotifyScriptsChanged() {
        mAdapter.onDataUpdated();
    }

    public void OnUserScriptLoaded(boolean result, String error) {
        if (result == false) {
            Toast toast = Toast.makeText(getContext(), error, Toast.LENGTH_LONG);
            toast.show();
        }
    }
}