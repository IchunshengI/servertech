// Contains all type definitions for out HTTP and websocket APIs

export enum ErrorId {
  BadRequest = "BAD_REQUEST",
  LoginFailed = "LOGIN_FAILED",
  UsernameExists = "USERNAME_EXISTS",
  EmailExists = "EMAIL_EXISTS",
}

export type APIErrorResponse = {
  id: string;
};

export type CreateAccountRequest = {
  username: string;
  email: string;
  password: string;
};

export type CreateAccountResponse =
  | { type: "ok" }
  | { type: ErrorId.UsernameExists }
  | { type: ErrorId.EmailExists };

export type LoginRequest = {
  email: string;
  password: string;
};

export type LoginResponse = { type: "ok" } | { type: ErrorId.LoginFailed };

export type User = {
  id: number;
  username: string;
};

export type ServerMessage = {
  id: string;
  user: User;
  content: string;
  timestamp: number; // miliseconds since epoch
};

export type ClientMessage = {
  content: string;
};

export type Room = {
  id: string;
  name: string;
  messages: ServerMessage[];
  hasMoreMessages: boolean;
};


export type Session = {  // 新增Session类型
  id: string;
  name: string;
  messages: ServerMessage[];
  hasMoreMessages: boolean;
};

// 修改HelloEvent，包含rooms和sessions
export type HelloEvent = {
  type: "hello";
  payload: {
    me: User;
    rooms: Room[];
    sessions: Session[];  // 添加sessions
  };
};

export type ServerMessagesEvent = {
  type: "serverMessages";
  payload: {
    roomId: string;
    messages: ServerMessage[];
  };
};

export type AnyServerEvent = 
  | HelloEvent 
  | ServerMessagesEvent 
  | ServerCreateSessionEvent 
  | ServerUpdateSessionEvent;

export type ClientMessagesEvent = {
  type: "clientMessages";
  payload: {
    roomId: string;
    messages: ClientMessage[];
  };
};

// 新增会话相关事件
export type ClientCreateSessionEvent = {
  type: "clientCreateSession";  // 创建新会话事件
  payload: {
    sessionId: string;  // 会话ID
    messages: ClientMessage[];
  };
};


export type ClientUpdateSessionEvent = {
  type: "clientUpdateSession";  // 更新会话事件
  payload: {
    sessionId: string;  // 会话ID
    messages: ClientMessage[];
  };
};

// 服务端返回的会话事件
export type ServerCreateSessionEvent = {
  type: "serverCreateSession";  // 服务端创建会话响应
  payload: {
    sessionId: string;  // 新创建的会话ID
    messages: ServerMessage[];
  };
};

export type ServerUpdateSessionEvent = {
  type: "serverUpdateSession";  // 服务端更新会话响应
  payload: {
    sessionId: string;  // 被更新的会话ID
    messages: ServerMessage[];
  };
};

//客户端设置API密钥事件
export interface ClientSetApikeyEvent {
  type: "clientSetApikey";
  payload: {
    key: string;
    sessionId?: string;  // 复用现有会话ID
  };
}

export type AnyClientEvent = 
  | ClientMessagesEvent
  | ClientCreateSessionEvent
  | ClientUpdateSessionEvent
  | ClientSetApikeyEvent;//联合类型
