import { defineConfig } from 'vitepress'

export default defineConfig({
  lang: 'zh-CN',
  title: 'Lele Tools',
  description: '乐乐的工具箱 — 60+ 实用工具集成的跨平台桌面应用',
  cleanUrls: true,
  lastUpdated: true,

  head: [
    ['meta', { name: 'theme-color', content: '#228be6' }],
    ['meta', { name: 'keywords', content: 'Lele Tools,乐乐工具箱,Qt6,跨平台,桌面工具,开源' }],
  ],

  themeConfig: {
    logo: '/logo.png',
    siteTitle: 'Lele Tools',

    nav: [
      { text: '首页', link: '/' },
      { text: '功能', link: '/features' },
      { text: '安装', link: '/install' },
      { text: '使用指南', link: '/guide/' },
      { text: '常见问题', link: '/faq' },
      { text: '下载', link: 'https://github.com/duhbbx/lele-tools/releases' },
    ],

    sidebar: {
      '/guide/': [
        {
          text: '使用指南',
          items: [
            { text: '概览', link: '/guide/' },
            { text: '主界面与基础操作', link: '/guide/basic' },
            { text: '编码与格式化工具', link: '/guide/format' },
            { text: '网络工具', link: '/guide/network' },
            { text: '文件与文本工具', link: '/guide/files' },
            { text: '图像工具', link: '/guide/image' },
            { text: '数据库工具', link: '/guide/database' },
            { text: 'PDF 工具', link: '/guide/pdf' },
            { text: '富文本笔记', link: '/guide/notepad' },
            { text: '工资计算器', link: '/guide/salary' },
            { text: 'OSS 图床', link: '/guide/oss' },
          ],
        },
      ],
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/duhbbx/lele-tools' },
    ],

    footer: {
      message: 'MIT 协议开源 · 武汉斯凯勒网络科技有限公司',
      copyright: '© 2025-2026 duhbbx · <a href="https://www.skyler.uno">www.skyler.uno</a>',
    },

    search: {
      provider: 'local',
      options: {
        translations: {
          button: { buttonText: '搜索文档', buttonAriaLabel: '搜索' },
          modal: {
            displayDetails: '显示详情',
            resetButtonTitle: '清除',
            backButtonTitle: '返回',
            noResultsText: '没有相关结果',
            footer: {
              selectText: '选择',
              navigateText: '切换',
              closeText: '关闭',
            },
          },
        },
      },
    },

    outline: { label: '本页目录', level: [2, 3] },
    docFooter: { prev: '上一页', next: '下一页' },
    lastUpdatedText: '上次更新',
    returnToTopLabel: '回到顶部',
    sidebarMenuLabel: '菜单',
    darkModeSwitchLabel: '主题',
    lightModeSwitchTitle: '切换到浅色模式',
    darkModeSwitchTitle: '切换到深色模式',
  },
})
