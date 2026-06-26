package com.amap.agenui.render.component.impl.list;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import com.amap.agenui.render.component.A2UIComponent;

/**
 * Thin RecyclerView holder that hosts an A2UIComponent's real Android view as its
 * single child.
 *
 * Why a holder + FrameLayout shell rather than using component.view directly as
 * itemView:
 * 1. RecyclerView mutates itemView's LayoutParams. We do NOT want that mutation
 *    to happen on component.view (which Component HAS-A and uses outside the RV
 *    context too). The shell absorbs all RV-level layout mutations.
 * 2. ViewHolder reuse: multiple A2UIComponents may pass through this same shell
 *    over the lifetime of the RV. attach()/detach() swaps the inner child without
 *    affecting the shell.
 *
 * The shell does NOT carry any per-component layout: child position inside the
 * RV is set by YogaLayoutManager.layoutDecorated(itemView, frame.left, frame.top,
 * ...). Inside the shell, the component view is positioned at (0,0) with the
 * yoga-computed size (see attach()).
 */
public class ComponentViewHolder extends RecyclerView.ViewHolder {

    @Nullable
    private A2UIComponent current;

    public ComponentViewHolder(@NonNull FrameLayout shell) {
        super(shell);
    }

    /**
     * Bind a component to this holder. Idempotent: if already bound to the same
     * component, no-op. Otherwise:
     * 1. Detach any previously bound component view from this holder.
     * 2. Force-detach the incoming component view from any other parent
     *    (covers RV-recycle race where the same component view was attached to
     *    a sibling holder still in the recycler pool — solves AND-P0-5).
     * 3. Reset the component view's LayoutParams to (0, 0, yogaW, yogaH) — RV
     *    relies on layoutDecorated for positioning, the inner view must not
     *    carry yoga x/y as margins (solves AND-P0-2 double-offset).
     * 4. Add the component view as the shell's only child.
     */
    void attach(@NonNull A2UIComponent component) {
        if (current == component) {
            return;
        }
        FrameLayout shell = (FrameLayout) itemView;

        // 1. Detach previous occupant from this holder.
        if (shell.getChildCount() > 0) {
            shell.removeAllViews();
        }
        current = component;

        // 2. Force-detach incoming view from any other parent.
        View childView = component.getView();
        if (childView == null) {
            return;
        }
        ViewParent oldParent = childView.getParent();
        if (oldParent instanceof ViewGroup) {
            ((ViewGroup) oldParent).removeView(childView);
        }

        // 3. Fill the shell: the shell's own size is set by YogaLayoutManager
        //    (measureChildExactly + layoutDecorated → frame width × height), so
        //    MATCH_PARENT makes the inner view match the yoga frame exactly —
        //    equivalent to YogaAbsoluteLayout.onMeasure using EXACTLY(yogaW, yogaH).
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);
        lp.leftMargin = 0;
        lp.topMargin = 0;
        childView.setLayoutParams(lp);

        // 4. Add as only child.
        shell.addView(childView);
    }

    /** Clear current binding. Called by adapter.onViewRecycled. */
    void detach() {
        current = null;
        ((FrameLayout) itemView).removeAllViews();
    }

    @Nullable
    A2UIComponent getCurrent() {
        return current;
    }
}
