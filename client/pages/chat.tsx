import Head from "@/components/Head";
import Header from "@/components/Header";
import RoomEntry from "@/components/RoomEntry";
import { useCallback, useEffect, useReducer, useRef ,useState} from "react";
import {
  parseWebsocketMessage,
  serializeMessagesEvent,
  serializeExitEvent,
  serializeCreateSessionEvent,
  serializeUpdateSessionEvent,
} from "@/lib/apiSerialization";
import { ServerMessage, Room, User } from "@/lib/apiTypes";
import { MyMessage, OtherUserMessage } from "@/components/Message";
import MessageInputBar from "@/components/MessageInputBar";
import autoAnimate from "@formkit/auto-animate";
import { useRouter } from "next/router";
import { clearHasAuth } from "@/lib/hasAuth";

// 定义 Session 类型
type Session = {
  id: string;
  name: string;
  messages: ServerMessage[];
  isTemp?: boolean; // 新增字段
};


// State 类型
type State = {
  currentUser: User | null;
  loading: boolean;
  rooms: Record<string, Room>;
  sessions: Record<string, Session>;
  currentRoomId: string | null;
  currentSessionId: string | null;
};

// Action 类型
type SetInitialStateAction = { type: "set_initial_state"; payload: { currentUser: User; rooms: Room[] } };
type AddMessagesAction = { type: "add_messages"; payload: { roomId: string; messages: ServerMessage[] } };
type SetCurrentRoomAction = { type: "set_current_room"; payload: { roomId: string | null } };
type AddSessionAction = { type: "add_session"; payload: { session: Session } };
type AddSessionMessagesAction = { type: "add_session_messages"; payload: { sessionId: string; messages: ServerMessage[] } };
type SetCurrentSessionAction = { type: "set_current_session"; payload: { sessionId: string | null } };
type RenameSessionAction = {
  type: "rename_session";
  payload: { sessionId: string; newName: string };
};
type RemoveSessionAction = { type: "remove_session"; payload: { sessionId: string } };
type Action =
  | SetInitialStateAction
  | AddMessagesAction
  | SetCurrentRoomAction
  | AddSessionAction
  | AddSessionMessagesAction
  | SetCurrentSessionAction
  | RenameSessionAction
  | RemoveSessionAction;

// 辅助：合并消息
const addNewMessages = (messages: ServerMessage[], newMessages: ServerMessage[]): ServerMessage[] => {
  // 先把旧消息的 id 收集到集合中
  const existingIds = new Set(messages.map(m => m.id));
  // 过滤掉已经存在的 msg
  const filtered = newMessages.filter(m => !existingIds.has(m.id));
  // 旧代码里有个 reverse，保持原来的时间顺序
  filtered.reverse();
  return filtered.concat(messages);
};

// Reducer
function reducer(state: State, action: Action): State {
  const { type, payload } = action;
  switch (type) {
    case "set_initial_state":
      const roomsMap: Record<string, Room> = {};
      payload.rooms.forEach(r => (roomsMap[r.id] = r));
      return {
        currentUser: payload.currentUser,
        loading: false,
        rooms: roomsMap,
        sessions: {},
        currentRoomId: payload.rooms[0]?.id || null,
        currentSessionId: null,
      };
    case "add_messages":
      if (!payload.roomId) return state;
      const room = state.rooms[payload.roomId];
      if (!room) return state;
      return {
        ...state,
        rooms: {
          ...state.rooms,
          [payload.roomId]: {
            ...room,
            messages: addNewMessages(room.messages, payload.messages),
          },
        },
      };
    case "set_current_room":
      return { ...state, currentRoomId: payload.roomId, currentSessionId: null };
    case "add_session":
      return {
        ...state,
        sessions: { ...state.sessions, [payload.session.id]: payload.session },
        currentSessionId: payload.session.id,
        currentRoomId: null,
      };
      case "add_session_messages": {
        const { sessionId, messages } = action.payload;
        const session = state.sessions[sessionId];
      
        const newMessages = session ? [...session.messages, ...messages] : messages;
      
        return {
          ...state,
          sessions: {
            ...state.sessions,
            [sessionId]: {
              id: sessionId,
              name: session?.name || `新会话 (${sessionId})`,
              messages: newMessages,
            },
          },
        };
      }
    case "set_current_session":
      return { ...state, currentSessionId: payload.sessionId, currentRoomId: null };
    case "rename_session": {
      const { sessionId, newName } = payload;
      const session = state.sessions[sessionId];
      if (!session) return state;
      return {
        ...state,
        sessions: {
          ...state.sessions,
          [sessionId]: {
            ...session,
            name: newName,
          },
        },
      };
    }
    case "remove_session": {
      const { sessionId } = payload;
      // 用解构拷贝然后 delete，保持 immutable
      const newSessions = { ...state.sessions };
      delete newSessions[sessionId];
      return {
        ...state,
        sessions: newSessions,
        // 如果删掉的是当前会话，重置 currentSessionId
        currentSessionId: state.currentSessionId === sessionId ? null : state.currentSessionId,
      };
    }
    default:
      return state;
  }
}

// Message 组件
const Message = ({ userId, username, content, timestamp, currentUserId }: { userId: number; username: string; content: string; timestamp: number; currentUserId: number; }) => {
  if (userId === currentUserId) return <MyMessage content={content} timestamp={timestamp} />;
  return <OtherUserMessage content={content} timestamp={timestamp} username={username} />;
};

// ChatScreen 组件

export const ChatScreen = ({
  rooms,
  sessions,
  currentRoom,
  currentSession,
  currentUserId,
  onClickRoom,
  onClickSession,
  onMessage,
  dispatch,
}: {
  rooms: Room[];
  sessions: Session[];
  currentRoom: Room | null;
  currentSession: Session | null;
  currentUserId: number;
  onClickRoom: (roomId: string) => void;
  onClickSession: (sessionId: string) => void;
  onMessage: (msg: string) => void;
  dispatch: React.Dispatch<Action>;
}) => {
  const isSession = Boolean(currentSession);
  const messagesRef = useCallback(elm => elm && autoAnimate(elm), []);
  const listRef = useCallback(elm => elm && autoAnimate(elm), []);

 // const currentMessages = isSession


  
  // 获取当前的消息列表（会话为正序，房间为倒序）
  const currentMessages = isSession
    ? [...(currentSession?.messages || [])]
    : [...(currentRoom?.messages || [])].reverse();

  const [editingId, setEditingId] = useState<string | null>(null);

  // 参考对象，用于自动滚动到底部
  const messageEndRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    // 每当消息更新时，自动滚动到底部
    messageEndRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [currentMessages]); // currentMessages 改变时执行

  const handleDoubleClick = useCallback((id: string) => {

    if (document.activeElement?.tagName === 'INPUT') return;
    // 如果是会话，允许修改
    if (sessions.some(session => session.id === id)) {
      setEditingId(id);
    }
  }, [sessions]);

  return (
    <div className="flex-1 flex min-h-0" style={{ borderTop: "1px solid var(--boost-light-grey)" }}>
      {/* 左侧：宽度15% */}
      <div className="w-[15%] flex flex-col overflow-y-scroll" ref={listRef}>
        {/* 房间列表 */}
        {rooms.map(({ id, name, messages }) => (
          <RoomEntry
            key={id}
            id={id}
            name={name}
            selected={!currentSession && id === currentRoom?.id}
            lastMessage={messages[0]?.content || "无历史消息"}
            onClick={onClickRoom}
            isRoom={true} // 房间不可修改
          />
        ))}

        {/* 分隔线 + 新建会话按钮 */}
        <div className="p-2 border-t border-gray-300">
          <button
            className="w-full bg-gray-800 hover:bg-gray-700 text-white px-3 py-2 rounded mb-2"
            onClick={() => onClickSession("__new__")}
          >
            新建会话
          </button>
          {/* 会话列表 */}
          {sessions.map(session => (
            <RoomEntry
              key={session.id}
              id={session.id}
              name={session.isTemp ? `${session.name}（未保存）` : session.name}
              selected={session.id === currentSession?.id}
              lastMessage={session.messages[0]?.content || "无历史消息"}
              onClick={onClickSession}
              onDoubleClick={() => handleDoubleClick(session.id)} // 会话双击修改名称
              isRoom={false} // 会话可修改
              onRename={(id, newName) => dispatch({ type: "rename_session", payload: { sessionId: id, newName } })}
            />
          ))}
        </div>
      </div>

      {/* 右侧：消息区 */}
      <div className="flex-[3] flex flex-col">
        <div
          //className="flex-1 flex flex-col-reverse p-5 overflow-y-scroll"
          //className="flex-1 flex flex-col p-5 overflow-y-scroll"
          className="flex-1 flex flex-col justify-end p-5 overflow-y-scroll"
          style={{ backgroundColor: "var(--boost-light-grey)" }}
          ref={messagesRef}
        >
          {currentMessages.length === 0 ? (
            <p>暂时没有历史消息</p>
          ) : (
            currentMessages.map(msg => (
              <Message
                key={msg.id}
                userId={msg.user.id}
                username={msg.user.username}
                content={msg.content}
                timestamp={msg.timestamp}
                currentUserId={currentUserId}
              />
            ))
          )}
        </div>
        <MessageInputBar onMessage={onMessage} />
      </div>
    </div>
  );
};

// ChatPage 主组件
export default function ChatPage() {
  const nextSessionIndex = useRef(1);
  const tempSessionCounter = useRef(1);   // 专门给“临时会话”用
  const [state, dispatch] = useReducer(reducer, {
    currentUser: null,
    loading: true,
    rooms: {},
    sessions: {},
    currentRoomId: null,
    currentSessionId: null,
  });

  const router = useRouter();
  const websocketRef = useRef<WebSocket>(null);

  const onMessageTyped = useCallback(
    (msg: string) => {
      const { currentSessionId, sessions, currentRoomId } = state;
      let handled = false;
  
      if (currentSessionId) {
        const sess = sessions[currentSessionId];
        const isTemp = currentSessionId.startsWith("temp");
        const isFirstMessage = !sess;
  
        if (isTemp && sess && sess.isTemp) {
          const oldId = currentSessionId;
          const nextIndex = nextSessionIndex.current++;
          const newId = `session${nextIndex}`;
          const newName = `会话 ${nextIndex}`;
          const newMessages = [
            {
              id: Date.now().toString(),
              content: msg,
              timestamp: Date.now(),
              user: state.currentUser!,
            },
          ];
        
          // 删除旧的临时会话，添加新的正式会话
          dispatch({
            type: "add_session",
            payload: {
              session: {
                id: newId,
                name: newName,
                messages: newMessages,
              },
            },
          });
        
          dispatch({
            type: "set_current_session",
            payload: { sessionId: newId },
          });
        
          // 移除临时会话（可选，如果你想清理 state.sessions 中的临时项）
          //delete state.sessions[oldId];
          dispatch({ type: "remove_session", payload: { sessionId: oldId } });
        
          // 可选：通知服务器创建会话
          websocketRef.current?.send(serializeCreateSessionEvent(newId, { content: msg }));
        
          handled = true;
        } else if (sess) {
          // 向已有会话发送消息
          dispatch({
            type: "add_session_messages",
            payload: {
              sessionId: currentSessionId,
              messages: [
                {
                  id: Date.now().toString(),
                  content: msg,
                  timestamp: Date.now(),
                  user: state.currentUser!,
                },
              ],
            },
          });
  
          // 可选：发送到服务器
          websocketRef.current?.send(serializeUpdateSessionEvent(currentSessionId, { content: msg }));
  
          handled = true;
        }
      }
  
      // 如果没有处理掉消息，说明不是会话，尝试作为房间消息发送
      if (!handled && currentRoomId) {
        const evt = serializeMessagesEvent(currentRoomId, { content: msg });
        websocketRef.current?.send(evt);
      }
    },
    [state]
  );
  

  // 点击房间
  const onClickRoom = useCallback(roomId => dispatch({ type: "set_current_room", payload: { roomId } }), []);
  // 点击会话
  const onClickSession = useCallback((sessionId: string) => {
    if (sessionId === "__new__") {
      // 分配一个临时 ID
      const idx = tempSessionCounter.current++;
      const tempId = `temp${idx}`;
      // 立刻加入一个临时会话到 sessions
      dispatch({
        type: "add_session",
        payload: {
          session: {
            id: tempId,
            name: `临时会话 ${idx}`,
            messages: [],
            isTemp: true,
          },
        },
      });
      // 切换到这个临时会话
      dispatch({ type: "set_current_session", payload: { sessionId: tempId } });
    } else {
      dispatch({ type: "set_current_session", payload: { sessionId } });
    }
  }, []);

  // WebSocket 初始化与事件
  useEffect(() => {
    websocketRef.current = new WebSocket(process.env.NEXT_PUBLIC_WEBSOCKET_URL || `ws://${location.hostname}:${location.port}/api/ws`);
    websocketRef.current.addEventListener("message", ev => {
      const { type, payload } = parseWebsocketMessage(ev.data);
      if (type === "hello") dispatch({ type: "set_initial_state", payload: { currentUser: payload.me, rooms: payload.rooms } });
      else if (type === "serverMessages") dispatch({ type: "add_messages", payload: { roomId: payload.roomId, messages: payload.messages } });
      else if (type === "serverUpdateSession")
      {
        const { sessionId, messages } = payload;
        // 更新对应会话的消息
        dispatch({
          type: "add_session_messages",
          payload: { sessionId, messages },
        });
      }
      
    });
    websocketRef.current.addEventListener("close", ev => {
      if (ev.code === 1008) { clearHasAuth(); router.replace("/login"); }
    });
    return () => websocketRef.current?.close();
  }, [router]);

  // 退出登录处理
  const handleLogout = useCallback(() => {
    if (websocketRef.current?.readyState === WebSocket.OPEN) websocketRef.current.send(serializeExitEvent());
    clearHasAuth();
    router.push("/login");
  }, [router]);

  if (state.loading) return <p>Loading...</p>;

  const rooms = Object.values(state.rooms);
  const sessions = Object.values(state.sessions);
  const currentRoom = state.currentRoomId ? state.rooms[state.currentRoomId] : null;
  const currentSession = state.currentSessionId ? state.sessions[state.currentSessionId] : null;

  return (
    <>
      <Head />
      <div className="flex flex-col h-full relative">
        <button onClick={handleLogout} className="absolute top-4 right-4 z-10 bg-red-500 hover:bg-red-600 text-white px-4 py-2 rounded">退出登录</button>
        <ChatScreen
          rooms={rooms}
          sessions={sessions}
          currentRoom={currentRoom}
          currentSession={currentSession}
          currentUserId={state.currentUser!.id}
          onClickRoom={onClickRoom}
          onClickSession={onClickSession}
          onMessage={onMessageTyped}
          dispatch={dispatch}
        />
      </div>
    </>
  );
}
