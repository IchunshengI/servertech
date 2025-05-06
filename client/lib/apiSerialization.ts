import {
  AnyServerEvent,
  ClientMessagesEvent,
  ClientMessage,
  ClientCreateSessionEvent,
  ClientUpdateSessionEvent,
  ServerCreateSessionEvent,
  ServerUpdateSessionEvent,
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
