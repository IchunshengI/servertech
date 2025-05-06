import NameAvatar from "@/components/NameAvatar";
//import { useCallback } from "react";
import { useState, useCallback } from "react";
// A clickable chat room card
export default function RoomEntry({
  id,
  name,
  lastMessage,
  selected,
  onClick,
  onDoubleClick,
  isRoom,
  onRename,
}: {
  id: string;
  name: string;
  lastMessage: string;
  selected: boolean;
  onClick: (roomId: string) => void;
  onDoubleClick?: () => void; // 可选的双击事件
  isRoom: boolean; // 判断是否是房间
  onRename?: (id: string, newName: string) => void;
}) {
  const cardStyle = selected
    ? { backgroundColor: "var(--boost-light-grey)" }
    : {};
  const onClickCallback = useCallback(() => onClick(id), [onClick, id]);

  return (
    <div
      className="flex pt-2 pl-3 pr-3 cursor-pointer overflow-hidden"
      onClick={onClickCallback}
      onDoubleClick={(e) => { // 修改这里
        e.stopPropagation();  // 阻止事件冒泡
        onDoubleClick?.();
      }}
    >
      <div
        className="flex-1 flex p-4 rounded-lg overflow-hidden"
        style={cardStyle}
      >
        <div className="pr-3 flex flex-col justify-center">
          <NameAvatar name={name} />
        </div>
        <div className="flex-1 flex flex-col overflow-hidden">
          <p className="m-0 font-bold pb-2" data-testid="room-name">
            {isRoom ? name : <EditableName name={name} id={id} onRename={onRename!} />}
          </p>
          <p className="m-0 overflow-hidden text-ellipsis whitespace-nowrap">
            {lastMessage}
          </p>
        </div>
      </div>
    </div>
  );
}


const EditableName = ({ name, id, onRename }: { name: string; id: string;
  onRename: (id: string, newName: string) => void;
 }) => {
  const [isEditing, setIsEditing] = useState(false);
  const [newName, setNewName] = useState(name);

  const handleBlur = () => {
    setIsEditing(false);
    // 这里可以触发一个修改会话名称的事件
    // 比如 dispatch 修改会话名称的 action
    onRename(id, newName);
    console.log(`修改会话名称为: ${newName}`); // 你可以在这里更新名称
  };

  const handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    setNewName(e.target.value);
  };

  const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Enter") {
      e.currentTarget.blur(); // 提交并触发 blur 事件
    } else if (e.key === "Escape") {
      setIsEditing(false);
      setNewName(name); // 撤销修改
    }
  };

  if (isEditing) {
    return (
      <input
        type="text"
        className="border border-gray-300 rounded px-1 py-0.5 text-sm w-full"
        value={newName}
        onChange={(e) => setNewName(e.target.value)}
        onBlur={handleBlur}
        onKeyDown={handleKeyDown}
        autoFocus
      />
    );
  }

  return (
    <span
      className="hover:underline cursor-text"
      onClick={() => setIsEditing(true)}
    >
      {name}
    </span>
  );
};