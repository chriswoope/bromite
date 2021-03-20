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

import android.content.Context;
import android.content.Intent;
import android.provider.Browser;
import android.net.Uri;
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
import org.chromium.base.ContextUtils;
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
            menuItems.add(buildMenuListItem(R.string.scripts_open_url, 0, 0, info.UrlSource != null &&
                                                                        info.UrlSource.isEmpty() == false));
            menuItems.add(buildMenuListItem(R.string.scripts_view_source, 0, 0, true));

            ListMenu.Delegate delegate = (model) -> {
                int textId = model.get(ListMenuItemProperties.TITLE_ID);
                if (textId == R.string.remove) {
                    UserScriptsBridge.RemoveScript(info.Key);
                } else if (textId == R.string.scripts_view_source) {
                    UserScriptsBridge.getUtils().openSourceFile(info.Key);
                } else if (textId == R.string.scripts_open_url) {
                    Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(info.UrlSource));
                    intent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
                    intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
                    intent.setPackage(mContext.getPackageName());
                    mContext.startActivity(intent);
                }
            };
            ((ScriptInfoRowViewHolder) holder)
                    .setMenuButtonDelegate(() -> new BasicListMenu(mContext, menuItems, delegate));
            ((ScriptInfoRowViewHolder) holder)
                    .setItemListener(new ScriptListBaseAdapter.ItemClickListener() {
                        @Override
                        public void onScriptOnOff(boolean Enabled) {
                            UserScriptsBridge.SetScriptEnabled(info.Key, Enabled);
                        }

                        @Override
                        public void onScriptClicked() {}
                    });
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
    private FragmentWindowAndroid mWindowAndroid;

    public ScriptListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAdapter = new ScriptListAdapter(context);
    }

    public void setWindowAndroid(FragmentWindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
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
            UserScriptsBridge.SelectAndAddScriptFromFile(mWindowAndroid);
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