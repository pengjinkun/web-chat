export type PeerType = 'p2p' | 'group'

export interface User {
  id: number
  username: string
  nickname: string
}

export interface Friend extends User {}

export interface GroupItem {
  id: number
  group_name: string
  owner_id: number
}

export interface ChatMessage {
  id: number
  fromUid: number
  toUid?: number
  groupId?: number
  content: string
  msgType: number
  createTime: string
}

export interface Session {
  type: PeerType
  id: number
  title: string
}

export type WsPayload = {
  type: string
  data: Record<string, unknown>
  timestamp?: number
  error?: string
}
