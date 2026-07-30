/** Top-level error boundary: renders a fallback instead of a blank page. */

import { Component, type ErrorInfo, type ReactNode } from "react";
import { AlertIcon } from "@/components/icons";

interface ErrorBoundaryProps {
  children: ReactNode;
}

interface ErrorBoundaryState {
  hasError: boolean;
  error: Error | null;
}

export class ErrorBoundary extends Component<ErrorBoundaryProps, ErrorBoundaryState> {
  state: ErrorBoundaryState = { hasError: false, error: null };

  static getDerivedStateFromError(error: Error): ErrorBoundaryState {
    return { hasError: true, error };
  }

  componentDidCatch(error: Error, info: ErrorInfo): void {
    // Surface the error in the console for debugging.
    console.error("AGenUI Studio UI error:", error, info.componentStack);
  }

  render(): ReactNode {
    if (this.state.hasError) {
      return (
        <div className="flex h-full flex-col items-center justify-center gap-3 bg-slate-50 p-8 text-center">
          <span className="flex h-12 w-12 items-center justify-center rounded-2xl bg-red-50 text-red-500">
            <AlertIcon size={22} />
          </span>
          <div>
            <p className="text-sm font-semibold text-slate-700">
              Something went wrong in the UI
            </p>
            <p className="mt-1 max-w-md break-all font-mono text-xs text-slate-400">
              {this.state.error?.message}
            </p>
          </div>
          <button
            type="button"
            onClick={() => window.location.reload()}
            className="rounded-lg bg-brand-500 px-4 py-2 text-sm font-medium text-white transition hover:bg-brand-600"
          >
            Reload page
          </button>
        </div>
      );
    }
    return this.props.children;
  }
}
