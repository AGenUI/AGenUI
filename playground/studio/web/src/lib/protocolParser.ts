/** Real-time parsing of the model's token stream.
 *
 * The model interleaves reasoning text with one or two ```json fenced blocks
 * (updateComponents, then updateDataModel). This module separates them so the
 * reasoning can be shown in a "thinking" area while the protocol JSON streams
 * into the editor. The whole buffer is re-parsed on each (throttled) tick, so
 * classification self-corrects as more of a block arrives.
 */

export interface ParsedStream {
  /** Reasoning text outside any ```json block. */
  thinking: string;
  /** Partial/full updateComponents JSON text. */
  components: string;
  /** Partial/full updateDataModel JSON text. */
  datamodel: string;
  /** Whether any protocol JSON block has started. */
  hasProtocol: boolean;
}

export function parseStream(buffer: string): ParsedStream {
  const result: ParsedStream = {
    thinking: "",
    components: "",
    datamodel: "",
    hasProtocol: false,
  };
  const closedBlocks: string[] = [];

  // Collect and strip closed ```json ... ``` blocks.
  let thinking = buffer.replace(
    /```json\s*\r?\n([\s\S]*?)\r?\n?\s*```/gi,
    (_full, body: string) => {
      closedBlocks.push(body);
      return "";
    },
  );

  // A trailing ```json without a closing fence is a block still streaming.
  let unclosed = "";
  const unclosedIdx = thinking.toLowerCase().lastIndexOf("```json");
  if (unclosedIdx !== -1) {
    unclosed = thinking.slice(unclosedIdx + 7).replace(/^\s*\r?\n/, "");
    thinking = thinking.slice(0, unclosedIdx);
  }

  const allBlocks = unclosed ? [...closedBlocks, unclosed] : closedBlocks;

  result.thinking = thinking.replace(/`{3,}/g, "").trim();
  allBlocks.forEach((block, idx) => {
    let target: "components" | "datamodel";
    if (block.includes("updateComponents")) target = "components";
    else if (block.includes("updateDataModel")) target = "datamodel";
    // Key not yet streamed: route by order (components first, then datamodel).
    else target = idx === 0 ? "components" : "datamodel";
    result[target] += block;
    result.hasProtocol = true;
  });

  return result;
}
