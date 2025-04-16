import socket
import struct  # 用于解析网络字节序数据


def receive_exact(sock, length):
    """从 socket 接收指定长度的数据"""
    data = b""
    while len(data) < length:
        chunk = sock.recv(length - len(data))
        if not chunk:
            raise ConnectionError("连接中断，无法接收完整数据")
        data += chunk
    return data


if __name__ == "__main__":
    # 创建一个 socket 对象
    s = socket.socket()
    s.connect(('127.0.0.1', 8080))

    # 传输user_id, session_id, api_key
    user_id = input("user_id: ").encode()
    session_id = input("session_id: ").encode()
    api_key = input("api_key: ").encode()

    total_length = 4 + 4 + len(user_id) + 4 + len(session_id) + 4 + len(api_key)
    # 构造网络字节流
    send_data = struct.pack('!I', total_length) # 总长度
    send_data += struct.pack('!I', len(user_id))  # user_id 长度
    send_data += user_id 
    send_data += struct.pack('!I', len(session_id))  # session_id 长度
    send_data += session_id 
    send_data += struct.pack('!I', len(api_key))  # api_key 长度
    send_data += api_key
    s.send(send_data)

    try:
        while True:
            # 发送数据
            content = input("客户端要发送的信息：").encode()
            # 构造网络字节流
            send_data = struct.pack('!I', len(content))
            send_data += content 
            s.send(send_data)

            # 1. 接收总长度（4字节）
            total_length_bytes = receive_exact(s, 4)
            total_length = struct.unpack('!I', total_length_bytes)[0]
            # print(
            #     f"接收到的数据包长度: {total_length}, Debug: {total_length_bytes.hex()}")

            if total_length > 1024*1024:
                raise ValueError(f"无效的总长度")

            # 2. 接收完整数据包（总长度-4字节）
            data = receive_exact(s, total_length - 4)
            offset = 0

            # 3. 解析content长度（4字节）
            content_length = struct.unpack('!I', data[offset:offset+4])[0]
            offset += 4

            # 4. 提取content数据
            response_content = data[offset:offset+content_length]
            offset += content_length

            # print("回答长度:", content_length)
            print("回答:", response_content.decode('utf-8'))

            if total_length > content_length + 8:
                # 5. 解析reason长度（4字节）
                reason_length = struct.unpack('!I', data[offset:offset+4])[0]
                offset += 4

                # 6. 提取reason数据
                response_reason = data[offset:offset+reason_length]
                offset += reason_length

                # print("思考长度:", reason_length)
                print("思考:", response_reason.decode('utf-8'))

            # 验证数据完整性
            if offset != len(data):
                raise ValueError("数据包格式错误，存在冗余数据")

    except ConnectionError as e:
        print(f"连接异常断开: {e}")
    except struct.error as e:
        print(f"数据解析失败: {e}")
    except ValueError as e:
        print(f"数据校验错误: {e}")
    finally:
        s.close()
