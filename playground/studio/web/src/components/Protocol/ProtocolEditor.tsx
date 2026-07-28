/** CodeMirror 6 JSON editor for one A2UI payload (components or datamodel). */

import { json } from "@codemirror/lang-json";
import CodeMirror from "@uiw/react-codemirror";
import { useMemo } from "react";

interface ProtocolEditorProps {
  value: string;
  onChange: (value: string) => void;
  /** While streaming, the editor is driven by the token stream (read-only). */
  readOnly: boolean;
}

export function ProtocolEditor({ value, onChange, readOnly }: ProtocolEditorProps) {
  const extensions = useMemo(() => [json()], []);

  return (
    <CodeMirror
      value={value}
      onChange={onChange}
      extensions={extensions}
      editable={!readOnly}
      basicSetup={{
        lineNumbers: true,
        foldGutter: true,
        highlightActiveLine: !readOnly,
        bracketMatching: true,
      }}
      height="100%"
      style={{ height: "100%", fontSize: 12 }}
      placeholder={readOnly ? "Waiting for protocol output..." : "{ }"}
    />
  );
}
