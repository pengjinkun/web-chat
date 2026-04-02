import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useReducer,
  useRef,
  type ReactNode,
} from 'react'
import { chatReducer, initialState, keyForPeer, isCurrentSession } from './chatReducer'
import type { ChatAction, FriendApplyRow } from './chatReducer'
import type { ChatMessage, Friend, GroupItem, Session, User } from '../types'
import { createWsUrl } from '../ws'

type SendFn = (type: string, data?: Record<string, unknown>) => void

interface ChatContextValue {
  state: ReturnType<typeof chatReducer>
  dispatch: React.Dispatch<ChatAction>
  send: SendFn
  login: (username: string, password: string) => void
  register: (username: string, password: string, nickname?: string) => void
  openSession: (session: Session) => void
  logout: () => void
}

const ChatContext = createContext<ChatContextValue | null>(null)

function parseP2pMessage(data: Record<string, unknown>): ChatMessage {
  return {
    id: Number(data.id),
    fromUid: Number(data.fromUid),
    toUid: data.toUid !== undefined ? Number(data.toUid) : undefined,
    content: String(data.content ?? ''),
    msgType: Number(data.msgType ?? 1),
    createTime: String(data.createTime ?? ''),
  }
}

function parseGroupMessage(data: Record<string, unknown>): ChatMessage {
  return {
    id: Number(data.id),
    fromUid: Number(data.fromUid),
    groupId: Number(data.groupId),
    content: String(data.content ?? ''),
    msgType: Number(data.msgType ?? 1),
    createTime: String(data.createTime ?? ''),
  }
}

export function ChatProvider({ children }: { children: ReactNode }) {
  const [state, dispatch] = useReducer(chatReducer, initialState)
  const stateRef = useRef(state)
  stateRef.current = state

  const wsRef = useRef<WebSocket | null>(null)
  const sendRef = useRef<SendFn>(() => {})

  const send = useCallback<SendFn>((type, data = {}) => {
    const ws = wsRef.current
    if (!ws || ws.readyState !== WebSocket.OPEN) return
    ws.send(JSON.stringify({ type, data, timestamp: Date.now() }))
  }, [])

  sendRef.current = send

  const login = useCallback(
    (username: string, password: string) => {
      send('user.login', { username, password })
    },
    [send]
  )

  const register = useCallback(
    (username: string, password: string, nickname = '') => {
      send('user.register', { username, password, nickname })
    },
    [send]
  )

  const openSession = useCallback(
    (session: Session) => {
      dispatch({ type: 'SET_SESSION', payload: session })
      dispatch({ type: 'CLEAR_UNREAD_CURRENT' })
      const peerType = session.type
      const peerId = session.id
      send('msg.read', { peerType, peerId })
      send('msg.history', { peerType, peerId, limit: 200 })
    },
    [send]
  )

  const logout = useCallback(() => {
    dispatch({ type: 'RESET' })
  }, [])

  useEffect(() => {
    const url = createWsUrl()
    const ws = new WebSocket(url)
    wsRef.current = ws
    dispatch({ type: 'SET_CONNECTED', payload: false })

    const toast = (message: string, ok = false) => {
      window.dispatchEvent(new CustomEvent('chat-toast', { detail: { message, ok } }))
    }

    ws.onopen = () => {
      dispatch({ type: 'SET_CONNECTED', payload: true })
    }
    ws.onclose = () => {
      dispatch({ type: 'SET_CONNECTED', payload: false })
    }
    ws.onmessage = (ev) => {
      let msg: { type: string; data?: Record<string, unknown>; error?: string }
      try {
        msg = JSON.parse(String(ev.data))
      } catch {
        return
      }
      const { type, data = {}, error } = msg
      if (error) {
        toast(String(error))
        return
      }

      const st = stateRef.current

      if (type === 'user.login') {
        const user = data.user as User
        const token = String(data.token ?? '')
        const friends = (data.friends as Friend[]) ?? []
        const groups = (data.groups as GroupItem[]) ?? []
        const unread = (data.unread as { p2p: Record<string, number>; group: Record<string, number> }) ?? {
          p2p: {},
          group: {},
        }
        dispatch({
          type: 'LOGIN_OK',
          payload: {
            user,
            token,
            friends,
            groups,
            unread,
            applyBell: !!data.applyBell,
            pendingApplyCount: Number(data.pendingApplyCount ?? 0),
          },
        })
        sendRef.current('msg.offline', {})
        sendRef.current('friend.recommend', {})
        return
      }

      if (type === 'sync.init') {
        const friends = (data.friends as Friend[]) ?? []
        const groups = (data.groups as GroupItem[]) ?? []
        const unread = (data.unread as { p2p: Record<string, number>; group: Record<string, number> }) ?? {
          p2p: {},
          group: {},
        }
        dispatch({ type: 'SET_FRIENDS', payload: friends })
        dispatch({ type: 'SET_GROUPS', payload: groups })
        dispatch({ type: 'SET_UNREAD', payload: { p2p: unread.p2p, group: unread.group } })
        dispatch({
          type: 'SET_APPLY_BELL',
          payload: {
            show: !!data.applyBell,
            count: Number(data.pendingApplyCount ?? 0),
          },
        })
        return
      }

      if (type === 'friend.recommend') {
        const users = (data.users as Array<User & { onlineSeconds: number }>) ?? []
        dispatch({ type: 'SET_RECOMMEND', payload: users })
        return
      }

      if (type === 'friend.search') {
        const users = (data.users as User[]) ?? []
        dispatch({ type: 'SET_FRIEND_SEARCH', payload: users })
        return
      }

      if (type === 'msg.history') {
        const peerType = data.peerType as 'p2p' | 'group'
        const peerId = Number(data.peerId)
        const messages = (data.messages as ChatMessage[]) ?? []
        const key = keyForPeer(peerType, peerId)
        dispatch({ type: 'SET_MESSAGES', payload: { key, messages, merge: 'replace' } })
        return
      }

      if (type === 'chat.p2p') {
        const m = parseP2pMessage(data)
        const a = m.fromUid
        const b = m.toUid
        if (b === undefined) return
        const uid = stateRef.current.user?.id
        const other = uid === a ? b : a
        const key = keyForPeer('p2p', other)
        dispatch({ type: 'APPEND_MESSAGE', payload: { key, message: m } })
        if (
          uid &&
          m.msgType === 1 &&
          m.fromUid !== uid &&
          !isCurrentSession(stateRef.current, 'p2p', other)
        ) {
          dispatch({ type: 'INCR_UNREAD_P2P', payload: { fid: String(other) } })
        }
        return
      }

      if (type === 'chat.group') {
        const m = parseGroupMessage(data)
        const gid = m.groupId
        if (gid === undefined) return
        const key = keyForPeer('group', gid)
        dispatch({ type: 'APPEND_MESSAGE', payload: { key, message: m } })
        const uid = stateRef.current.user?.id
        if (
          uid &&
          m.msgType === 1 &&
          m.fromUid !== uid &&
          !isCurrentSession(stateRef.current, 'group', gid)
        ) {
          dispatch({ type: 'INCR_UNREAD_GROUP', payload: { gid: String(gid) } })
        }
        return
      }

      if (type === 'friend.agree') {
        const friend = data.friend as Friend
        if (friend) dispatch({ type: 'UPSERT_FRIENDS', payload: [friend] })
        sendRef.current('sync.init', {})
        sendRef.current('friend.apply.list', {})
        return
      }

      if (type === 'friend.apply.reject' && data.ok) {
        sendRef.current('friend.apply.list', {})
        return
      }

      if (type === 'notify.friend_apply') {
        dispatch({
          type: 'SET_APPLY_BELL',
          payload: { show: true, count: (st.pendingApplyCount ?? 0) + 1 },
        })
        return
      }

      if (type === 'friend.apply.read') {
        dispatch({ type: 'SET_APPLY_BELL', payload: { show: false } })
        return
      }

      if (type === 'friend.apply.list') {
        const list = (data.list as FriendApplyRow[]) ?? []
        dispatch({ type: 'SET_APPLY_LIST', payload: list })
        return
      }

      if (type === 'group.create') {
        const g = data.group as GroupItem
        const sm = data.systemMessage as Record<string, unknown> | undefined
        if (g) {
          dispatch({ type: 'ADD_GROUP', payload: g })
          if (sm) {
            const m = parseGroupMessage(sm)
            dispatch({
              type: 'APPEND_MESSAGE',
              payload: { key: keyForPeer('group', g.id), message: m },
            })
          }
        }
        return
      }

      if (type === 'group.join') {
        const g = data.group as GroupItem
        if (g) dispatch({ type: 'ADD_GROUP', payload: g })
        return
      }

      if (type === 'user.register') {
        toast('注册成功，请登录', true)
        window.dispatchEvent(new CustomEvent('chat-nav', { detail: { to: '/login' } }))
      }
    }

    return () => {
      ws.close()
      wsRef.current = null
    }
  }, [])

  const value = useMemo(
    () => ({
      state,
      dispatch,
      send,
      login,
      register,
      openSession,
      logout,
    }),
    [state, send, login, register, openSession, logout]
  )

  return <ChatContext.Provider value={value}>{children}</ChatContext.Provider>
}

export function useChat(): ChatContextValue {
  const ctx = useContext(ChatContext)
  if (!ctx) throw new Error('useChat outside ChatProvider')
  return ctx
}
