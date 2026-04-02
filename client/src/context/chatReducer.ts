import type { ChatMessage, Friend, GroupItem, Session, User } from '../types'

export interface FriendApplyRow {
  id: number
  from_uid: number
  status: number
  create_time: string
  username: string
  nickname: string
}

export interface ChatState {
  user: User | null
  token: string | null
  connected: boolean
  friends: Friend[]
  groups: GroupItem[]
  currentSession: Session | null
  /** key: `p2p:${id}` | `group:${id}` -> messages (max 200) */
  messagesByKey: Record<string, ChatMessage[]>
  unreadP2p: Record<string, number>
  unreadGroup: Record<string, number>
  applyBell: boolean
  pendingApplyCount: number
  applyList: FriendApplyRow[]
  activeRecommend: Array<User & { onlineSeconds: number }>
  friendSearchResults: User[]
}

export const initialState: ChatState = {
  user: null,
  token: null,
  connected: false,
  friends: [],
  groups: [],
  currentSession: null,
  messagesByKey: {},
  unreadP2p: {},
  unreadGroup: {},
  applyBell: false,
  pendingApplyCount: 0,
  applyList: [],
  activeRecommend: [],
  friendSearchResults: [],
}

export type ChatAction =
  | { type: 'RESET' }
  | { type: 'SET_CONNECTED'; payload: boolean }
  | {
      type: 'LOGIN_OK'
      payload: {
        user: User
        token: string
        friends: Friend[]
        groups: GroupItem[]
        unread: { p2p: Record<string, number>; group: Record<string, number> }
        applyBell: boolean
        pendingApplyCount: number
      }
    }
  | { type: 'SET_SESSION'; payload: Session | null }
  | { type: 'CLEAR_UNREAD_CURRENT' }
  | { type: 'SET_FRIENDS'; payload: Friend[] }
  | { type: 'UPSERT_FRIENDS'; payload: Friend[] }
  | { type: 'SET_GROUPS'; payload: GroupItem[] }
  | { type: 'ADD_GROUP'; payload: GroupItem }
  | { type: 'SET_MESSAGES'; payload: { key: string; messages: ChatMessage[]; merge?: 'replace' | 'prepend' } }
  | { type: 'APPEND_MESSAGE'; payload: { key: string; message: ChatMessage } }
  | { type: 'SET_UNREAD'; payload: { p2p?: Record<string, number>; group?: Record<string, number> } }
  | { type: 'INCR_UNREAD_P2P'; payload: { fid: string; delta?: number } }
  | { type: 'INCR_UNREAD_GROUP'; payload: { gid: string; delta?: number } }
  | { type: 'SET_APPLY_BELL'; payload: { show: boolean; count?: number } }
  | { type: 'SET_APPLY_LIST'; payload: FriendApplyRow[] }
  | { type: 'SET_RECOMMEND'; payload: Array<User & { onlineSeconds: number }> }
  | { type: 'SET_FRIEND_SEARCH'; payload: User[] }

function trimMessages(list: ChatMessage[]): ChatMessage[] {
  if (list.length <= 200) return list
  return list.slice(list.length - 200)
}

export function chatReducer(state: ChatState, action: ChatAction): ChatState {
  switch (action.type) {
    case 'RESET':
      return { ...initialState }
    case 'SET_CONNECTED':
      return { ...state, connected: action.payload }
    case 'LOGIN_OK': {
      const { user, token, friends, groups, unread, applyBell, pendingApplyCount } = action.payload
      return {
        ...state,
        user,
        token,
        friends,
        groups,
        unreadP2p: unread.p2p ?? {},
        unreadGroup: unread.group ?? {},
        applyBell,
        pendingApplyCount,
      }
    }
    case 'SET_SESSION':
      return { ...state, currentSession: action.payload }
    case 'CLEAR_UNREAD_CURRENT': {
      if (!state.currentSession || !state.user) return state
      const k =
        state.currentSession.type === 'p2p'
          ? `p2p:${state.currentSession.id}`
          : `group:${state.currentSession.id}`
      if (state.currentSession.type === 'p2p') {
        const fid = String(state.currentSession.id)
        const next = { ...state.unreadP2p, [fid]: 0 }
        return { ...state, unreadP2p: next }
      }
      const gid = String(state.currentSession.id)
      const nextG = { ...state.unreadGroup, [gid]: 0 }
      return { ...state, unreadGroup: nextG }
    }
    case 'SET_FRIENDS':
      return { ...state, friends: action.payload }
    case 'UPSERT_FRIENDS': {
      const map = new Map(state.friends.map((f) => [f.id, f]))
      for (const f of action.payload) map.set(f.id, f)
      return { ...state, friends: Array.from(map.values()) }
    }
    case 'SET_GROUPS':
      return { ...state, groups: action.payload }
    case 'ADD_GROUP':
      if (state.groups.some((g) => g.id === action.payload.id)) return state
      return { ...state, groups: [...state.groups, action.payload] }
    case 'SET_MESSAGES': {
      const { key, messages, merge = 'replace' } = action.payload
      const prev = state.messagesByKey[key] ?? []
      let next: ChatMessage[]
      if (merge === 'prepend') {
        const byId = new Set(messages.map((m) => m.id))
        const rest = prev.filter((m) => !byId.has(m.id))
        next = trimMessages([...messages, ...rest])
      } else {
        next = trimMessages(messages)
      }
      return { ...state, messagesByKey: { ...state.messagesByKey, [key]: next } }
    }
    case 'APPEND_MESSAGE': {
      const { key, message } = action.payload
      const prev = state.messagesByKey[key] ?? []
      if (prev.some((m) => m.id === message.id)) return state
      const next = trimMessages([...prev, message])
      return { ...state, messagesByKey: { ...state.messagesByKey, [key]: next } }
    }
    case 'SET_UNREAD':
      return {
        ...state,
        unreadP2p: action.payload.p2p ?? state.unreadP2p,
        unreadGroup: action.payload.group ?? state.unreadGroup,
      }
    case 'INCR_UNREAD_P2P': {
      const d = action.payload.delta ?? 1
      const fid = action.payload.fid
      const cur = state.unreadP2p[fid] ?? 0
      return { ...state, unreadP2p: { ...state.unreadP2p, [fid]: cur + d } }
    }
    case 'INCR_UNREAD_GROUP': {
      const d = action.payload.delta ?? 1
      const gid = action.payload.gid
      const cur = state.unreadGroup[gid] ?? 0
      return { ...state, unreadGroup: { ...state.unreadGroup, [gid]: cur + d } }
    }
    case 'SET_APPLY_BELL':
      return {
        ...state,
        applyBell: action.payload.show,
        pendingApplyCount:
          action.payload.count !== undefined ? action.payload.count : state.pendingApplyCount,
      }
    case 'SET_APPLY_LIST':
      return { ...state, applyList: action.payload }
    case 'SET_RECOMMEND':
      return { ...state, activeRecommend: action.payload }
    case 'SET_FRIEND_SEARCH':
      return { ...state, friendSearchResults: action.payload }
    default:
      return state
  }
}

export function keyForPeer(type: 'p2p' | 'group', id: number): string {
  return type === 'p2p' ? `p2p:${id}` : `group:${id}`
}

export function isCurrentSession(
  state: ChatState,
  type: 'p2p' | 'group',
  id: number
): boolean {
  if (!state.currentSession) return false
  return state.currentSession.type === type && state.currentSession.id === id
}
