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

import android.content.Context;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityManager.AccessibilityStateChangeListener;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableListAdapter;
import org.chromium.components.browser_ui.widget.dragreorder.DragStateDelegate;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;

import java.util.ArrayList;
import java.util.List;

public class ScriptListBaseAdapter extends DragReorderableListAdapter<ScriptInfo> {

    interface ItemClickListener {
        void onScriptClicked(ScriptInfo item);
    }

    static class ScriptInfoRowViewHolder extends ViewHolder {
        private TextView mTitle;
        private TextView mDescription;
        private TextView mVersion;
        private TextView mFile;

        private ImageView mStartIcon;
        private ListMenuButton mMoreButton;

        ScriptInfoRowViewHolder(View view) {
            super(view);

            mTitle = view.findViewById(R.id.title);
            mDescription = view.findViewById(R.id.description);
            mVersion = view.findViewById(R.id.version);
            mFile = view.findViewById(R.id.file);

            mStartIcon = view.findViewById(R.id.icon_view);
            mMoreButton = view.findViewById(R.id.more);
        }

        protected void updateScriptInfo(ScriptInfo item) {
            mTitle.setText(item.Name);
            mDescription.setText(item.Description);
            mVersion.setText(item.Version);
            mFile.setText(item.Key);

            if (item.Enabled)
                setStartIcon(R.drawable.userscript_on);
            else
                setStartIcon(R.drawable.userscript_off);
        }

        void setStartIcon(@DrawableRes int iconResId) {
            mStartIcon.setVisibility(View.VISIBLE);
            mStartIcon.setImageResource(iconResId);
        }

        void setMenuButtonDelegate(@NonNull ListMenuButtonDelegate delegate) {
            mMoreButton.setVisibility(View.VISIBLE);
            mMoreButton.setDelegate(delegate);
            // Set item row end padding 0 when MenuButton is visible.
            ViewCompat.setPaddingRelative(itemView, ViewCompat.getPaddingStart(itemView),
                    itemView.getPaddingTop(), 0, itemView.getPaddingBottom());
        }

        void setItemClickListener(ScriptInfo item, @NonNull ItemClickListener listener) {
            itemView.setOnClickListener(view -> listener.onScriptClicked(item));
        }
    }

    private class ScriptDragStateDelegate implements DragStateDelegate {
        private AccessibilityManager mA11yManager;
        private AccessibilityStateChangeListener mA11yListener;
        private boolean mA11yEnabled;

        public ScriptDragStateDelegate() {
            mA11yManager =
                    (AccessibilityManager) mContext.getSystemService(Context.ACCESSIBILITY_SERVICE);
            mA11yEnabled = mA11yManager.isEnabled();
            mA11yListener = enabled -> {
                mA11yEnabled = enabled;
                notifyDataSetChanged();
            };
            mA11yManager.addAccessibilityStateChangeListener(mA11yListener);
        }

        @Override
        public boolean getDragEnabled() {
            return !mA11yEnabled;
        }

        @Override
        public boolean getDragActive() {
            return getDragEnabled();
        }

        @Override
        public void setA11yStateForTesting(boolean a11yEnabled) {
        }
    }

    ScriptListBaseAdapter(Context context) {
        super(context);
        setDragStateDelegate(new ScriptDragStateDelegate());
    }

    void showDragIndicatorInRow(ScriptInfoRowViewHolder holder) {
        // Quit if it's not applicable.
        if (getItemCount() <= 1 || !mDragStateDelegate.getDragEnabled()) return;

        assert mItemTouchHelper != null;
        //holder.setStartIcon(R.drawable.ic_drag_handle_grey600_24dp);
        holder.mStartIcon.setOnTouchListener((v, event) -> {
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                mItemTouchHelper.startDrag(holder);
            }
            return false;
        });
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup viewGroup, int i) {
        View row = LayoutInflater.from(viewGroup.getContext())
                           .inflate(R.layout.accept_script_item, viewGroup, false);
        return new ScriptInfoRowViewHolder(row);
    }

    @Override
    public void onBindViewHolder(ViewHolder viewHolder, int i) {
        ((ScriptInfoRowViewHolder) viewHolder).updateScriptInfo(mElements.get(i));
    }

    @Override
    protected void setOrder(List<ScriptInfo> order) {
        // String[] codes = new String[order.size()];
        // for (int i = 0; i < order.size(); i++) {
        //     codes[i] = order.get(i).getCode();
        // }
        //LanguagesManager.getInstance().setOrder(codes, false);
        //notifyDataSetChanged();
    }

    void setDisplayedScriptInfo(List<ScriptInfo> values) {
        mElements = new ArrayList<>(values);
        notifyDataSetChanged();
    }

    @Override
    protected boolean isActivelyDraggable(ViewHolder viewHolder) {
        return isPassivelyDraggable(viewHolder);
    }

    @Override
    protected boolean isPassivelyDraggable(ViewHolder viewHolder) {
        return viewHolder instanceof ScriptInfoRowViewHolder;
    }
}
