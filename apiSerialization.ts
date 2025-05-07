import {
  AnyServerEvent,
  ClientMessagesEvent,
  ClientMessage,
  ClientCreateSessionEvent,
  ClientUpdateSessionEvent,
  ServerCreateSessionEvent,
  ServerUpdateSessionEvent,
  ClientSetApikeyEvent
} from "@/lib/apiTypes";

// Contains functions to parse raw data into API types and vice-versa

export function parseWebsocketMessage(s: string): AnyServerEvent {
  // TODO: some validation here would be good
  return JSON.parse(s);
}

export function serializeMessagesEvent(
  roomId: string,
  message: ClientMessage,
): string {
  const evt: ClientMessagesEvent = {
    type: "clientMessages",
    payload: {
      roomId,
      messages: [message],
    },
  };
  return JSON.stringify(evt);
}

// 创建会话事件的序列化
export function serializeCreateSessionEvent(
  sessionId: string,
  message: ClientMessage
): string {
  const evt: ClientCreateSessionEvent = {
    type: "clientCreateSession",
    payload: {
      sessionId,
      messages: [message],
    },
  };
  return JSON.stringify(evt);
}

// 更新会话事件的序列化
export function serializeUpdateSessionEvent(
  sessionId: string,
  message: ClientMessage
): string {
  const evt: ClientUpdateSessionEvent = {
    type: "clientUpdateSession",
    payload: {
      sessionId,
      messages: [message],
    },
  };
  return JSON.stringify(evt);
}


export function serializeExitEvent() {
  return JSON.stringify({
    type: "clientExit",
    payload: {}
  });
}


export function serializeSetApikeyEvent(
  key: string
  ): string {
  const evt: ClientSetApikeyEvent = {
    type: "clientSetApikey", 
    payload: { 
      key,
      sessionId: getActiveSessionId() 
    }
  };
  return JSON.stringify(evt);
}

// 会话ID获取同步调整
function getActiveSessionId(): string | undefined {
  const session = localStorage.getItem('current_session');
  return session ? session.replace(/-/g, '_') : undefined; // 确保ID格式兼容
}