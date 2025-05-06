import { useCallback, useEffect, useRef } from "react";

// The input bar where the user types messages
export default function MessageInputBar({
  onMessage,
}: {
  onMessage: (msg: string) => void;
}) {
  const inputRef = useRef<HTMLInputElement>(null);

  const handleKeyDown = (event: React.KeyboardEvent<HTMLInputElement>) => {
    if (event.key === "Enter") {
      const messageContent = inputRef.current?.value;
      if (messageContent) {
        onMessage(messageContent);
        inputRef.current.value = "";
      }
    }
  };

  return (
    <div className="flex">
      <div
        className="flex-1 flex p-2"
        style={{ backgroundColor: "var(--boost-light-grey)" }}
      >
        <input
          type="text"
          className="flex-1 text-xl pl-4 pr-4 pt-2 pb-2 border-0 rounded-xl"
          placeholder="编辑消息..."
          ref={inputRef}
          onKeyDown={handleKeyDown} // ✅ 直接在输入框监听回车
        />
      </div>
    </div>
  );
}
