/** QR code card: vertical layout, 128px code for easy scanning. */

import { QRCodeSVG } from "qrcode.react";
import { useState } from "react";
import { CheckIcon, CopyIcon } from "@/components/icons";
import { copyText } from "@/lib/utils";

interface QrCodeCardProps {
  url: string;
}

export function QrCodeCard({ url }: QrCodeCardProps) {
  const [copied, setCopied] = useState(false);

  const handleCopy = async () => {
    if (await copyText(url)) {
      setCopied(true);
      window.setTimeout(() => setCopied(false), 1500);
    }
  };

  return (
    <div className="flex w-[172px] shrink-0 flex-col items-center gap-1.5 rounded-lg border border-slate-200 bg-white p-2.5">
      <div className="rounded-md border border-slate-100 bg-white p-1.5 shadow-sm">
        <QRCodeSVG value={url} size={152} level="M" />
      </div>
      <p className="text-center text-[10px] font-medium leading-tight text-slate-500">
        Scan with{" "}
        <a
          href="https://genui.amap.com/playground"
          target="_blank"
          rel="noopener noreferrer"
          title="Open AGenUI Playground"
          className="text-brand-500 transition hover:underline"
        >
          AGenUI Playground
        </a>
      </p>
      <button
        type="button"
        onClick={handleCopy}
        className="inline-flex items-center gap-1 rounded-md border border-slate-200 px-2 py-0.5 text-[10px] font-medium text-slate-500 transition hover:border-slate-300 hover:bg-slate-50"
      >
        {copied ? (
          <>
            <CheckIcon size={10} className="text-emerald-500" />
            Copied
          </>
        ) : (
          <>
            <CopyIcon size={10} />
            Copy URL
          </>
        )}
      </button>
    </div>
  );
}
