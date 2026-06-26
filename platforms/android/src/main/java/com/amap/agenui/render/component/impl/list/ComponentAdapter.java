package com.amap.agenui.render.component.impl.list;

import android.content.Context;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.amap.agenui.render.component.A2UIComponent;

import java.util.List;

/**
 * Adapter bridging the ListComponent.children list to RecyclerView.
 *
 * onBindViewHolder is the trigger point for lazy createView: when a position is
 * scrolled into the visible window (or RV's cache window), the adapter calls
 * child.createView(context, holder.itemView) which materializes the child view
 * (and recursively any descendants via createChildViews).
 *
 * Because ListComponent.getChildContainer() returns null, Surface.handleChildComponent
 * short-circuits at dispatch time and never calls createView on List children;
 * onBindViewHolder is the SOLE trigger for child view materialization.
 *
 * The createView idempotency guard inside A2UIComponent ensures it is safe to
 * call from every bind pass (only the first one runs onCreateView).
 */
public class ComponentAdapter extends RecyclerView.Adapter<ComponentViewHolder> {

    /**
     * Listener notified each time an item is bound by {@link #onBindViewHolder}.
     * Used by ListComponent to detect "bind == display".
     */
    public interface OnItemBindListener {
        /**
         * Called after the child has been attached to its holder during a bind pass.
         *
         * @param position the bound adapter position
         * @param child    the bound child component
         */
        void onItemBound(int position, @NonNull A2UIComponent child);
    }

    @NonNull
    private final List<A2UIComponent> children;

    private OnItemBindListener onItemBindListener;

    public ComponentAdapter(@NonNull List<A2UIComponent> children) {
        this.children = children;
    }

    public void setOnItemBindListener(OnItemBindListener listener) {
        this.onItemBindListener = listener;
    }

    @NonNull
    @Override
    public ComponentViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        // Shell is just a transparent FrameLayout; LayoutManager.layoutDecorated
        // will set the shell's frame inside the RV viewport.
        FrameLayout shell = new FrameLayout(parent.getContext());
        shell.setLayoutParams(new RecyclerView.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));
        return new ComponentViewHolder(shell);
    }

    @Override
    public void onBindViewHolder(@NonNull ComponentViewHolder holder, int position) {
        A2UIComponent child = children.get(position);
        Context context = holder.itemView.getContext();

        // Trigger lazy createView. Idempotent guard inside createView ensures this
        // runs onCreateView exactly once per component, regardless of how many
        // RV bind passes hit it.
        if (!child.isViewCreated()) {
            child.createView(context, (FrameLayout) holder.itemView);
        }
        holder.attach(child);

        // Bind == display: notify the listener so ListComponent can report display.
        if (onItemBindListener != null) {
            onItemBindListener.onItemBound(position, child);
        }
    }

    @Override
    public void onViewRecycled(@NonNull ComponentViewHolder holder) {
        super.onViewRecycled(holder);
        holder.detach();
    }

    @Override
    public int getItemCount() {
        return children.size();
    }
}
