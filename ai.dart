import 'dart:async';
import 'dart:ui'; 
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:flutter_markdown/flutter_markdown.dart';

void main() {
  runApp(const VaxpAdmiralAiUltimate());
}

class VaxpAdmiralAiUltimate extends StatelessWidget {
  const VaxpAdmiralAiUltimate({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Admiral AI Ultimate',
      debugShowCheckedModeBanner: false,
      theme: ThemeData.dark().copyWith(
        scaffoldBackgroundColor: Colors.transparent, // خلفية التطبيق شفافة
        canvasColor: Colors.transparent, // لضمان عدم وجود خلفيات بيضاء افتراضية
      ),
      home: const AdmiralAiScreen(),
    );
  }
}

class AdmiralAiScreen extends StatefulWidget {
  final String? initialQuery;
  const AdmiralAiScreen({super.key, this.initialQuery});

  @override
  State<AdmiralAiScreen> createState() => _AdmiralAiScreenState();
}

class _AdmiralAiScreenState extends State<AdmiralAiScreen> {
  final TextEditingController _controller = TextEditingController();
  final ScrollController _scrollController = ScrollController();
  
  String _fullResponse = "";
  String _displayedResponse = "";
  
  bool _isLoading = false;
  bool _isTyping = false;
  String _statusMessage = "Ready. Type 'ai: ...'";
  
  Timer? _typewriterTimer;

  @override
  void initState() {
    super.initState();
    if (widget.initialQuery != null && widget.initialQuery!.trim().isNotEmpty) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        final initial = widget.initialQuery!.trim();
        _controller.text = initial;
        fetchAiResponse(initial);
      });
    }
  }

  bool _checkForArabic(String text) {
    if (text.isEmpty) return false;
    return RegExp(r'[\u0600-\u06FF]').hasMatch(text);
  }

  Future<void> fetchAiResponse(String query) async {
    final cleanQuery = query.replaceFirst(RegExp(r'^ai:\s*', caseSensitive: false), '').trim();
    if (cleanQuery.isEmpty) return;

    _typewriterTimer?.cancel();
    
    setState(() {
      _isLoading = true;
      _isTyping = false;
      _fullResponse = "";
      _displayedResponse = "";
      _statusMessage = "Admiral is analyzing...";
    });

    try {
      final url = Uri.parse('https://text.pollinations.ai/${Uri.encodeComponent(cleanQuery)}');
      final response = await http.get(url);

      if (response.statusCode == 200) {
        _fullResponse = response.body;
        _startTypewriterEffect();
      } else {
        setState(() {
          _statusMessage = "Error: ${response.statusCode}";
          _isLoading = false;
        });
      }
    } catch (e) {
      setState(() {
        _statusMessage = "Network Error.";
        _isLoading = false;
      });
    }
  }

  void _startTypewriterEffect() {
    setState(() {
      _isLoading = false;
      _isTyping = true;
      _statusMessage = "Typing...";
    });

    int currentIndex = 0;
    const speed = Duration(milliseconds: 10);

    _typewriterTimer = Timer.periodic(speed, (timer) {
      if (currentIndex < _fullResponse.length) {
        setState(() {
          _displayedResponse += _fullResponse[currentIndex];
        });
        currentIndex++;
        
        if (_scrollController.hasClients) {
          _scrollController.jumpTo(_scrollController.position.maxScrollExtent);
        }
      } else {
        timer.cancel();
        setState(() {
          _isTyping = false;
          _statusMessage = "Done.";
        });
      }
    });
  }

  @override
  void dispose() {
    _typewriterTimer?.cancel();
    _controller.dispose();
    _scrollController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final bool isRtl = _checkForArabic(_displayedResponse);

    return Scaffold(
      backgroundColor: Colors.transparent, // جعل السقالات شفافة تماماً
      body: Center(
        child: ClipRRect( // قص التأثير الضبابي ليتناسب مع الحواف الدائرية
          borderRadius: BorderRadius.circular(18),
          child: BackdropFilter(
            // قوة التغويش (Blur)
            filter: ImageFilter.blur(sigmaX: 15.0, sigmaY: 15.0),
            child: Container(
              width: 800,
              height: 550,
              padding: const EdgeInsets.all(20),
              decoration: BoxDecoration(
                // لون الخلفية يجب أن يكون نصف شفاف (Opacity) لظهور الـ Blur
                color: const Color.fromARGB(82, 0, 0, 0), 
                borderRadius: BorderRadius.circular(18),
                border: Border.all(color: Colors.white.withOpacity(0.1)),
                boxShadow: [
                  BoxShadow(
                    color: Colors.black.withOpacity(0.2),
                    blurRadius: 20,
                    spreadRadius: 5,
                  )
                ],
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  // --- شريط العنوان العلوي ---
                  Row(
                    children: [
                      const Icon(Icons.smart_toy_outlined, color: Colors.blueAccent, size: 20),
                      const SizedBox(width: 10),
                      const Text("Admiral AI", style: TextStyle(color: Colors.white, fontWeight: FontWeight.bold)),
                      const Spacer(),
                      Container(
                        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                        decoration: BoxDecoration(
                          color: Colors.blueAccent.withOpacity(0.2),
                          borderRadius: BorderRadius.circular(4),
                        ),
                        child: const Text("BETA", style: TextStyle(color: Colors.blueAccent, fontSize: 10, fontWeight: FontWeight.bold)),
                      ),
                    ],
                  ),
                  const SizedBox(height: 15),

                  // --- حقل الإدخال ---
                  Container(
                    padding: const EdgeInsets.symmetric(horizontal: 15),
                    decoration: BoxDecoration(
                      color: const Color(0xFF252525).withOpacity(0.6), // تقليل الشفافية قليلاً هنا
                      borderRadius: BorderRadius.circular(12),
                      border: Border.all(color: Colors.white10),
                    ),
                    child: TextField(
                      controller: _controller,
                      style: const TextStyle(fontSize: 16, color: Colors.white, fontFamily: 'Monospace'),
                      cursorColor: Colors.blueAccent,
                      decoration: const InputDecoration(
                        icon: Icon(Icons.search_rounded, color: Colors.grey),
                        hintText: "Type 'ai: info about VAXP-OS'...",
                        hintStyle: TextStyle(color: Colors.white24),
                        border: InputBorder.none,
                        contentPadding: EdgeInsets.symmetric(vertical: 14),
                      ),
                      onSubmitted: (val) {
                        if (val.toLowerCase().startsWith("ai:")) {
                          fetchAiResponse(val);
                        }
                      },
                    ),
                  ),

                  const SizedBox(height: 20),

                  // --- منطقة النتائج ---
                  Expanded(
                    child: Container(
                      width: double.infinity,
                      padding: const EdgeInsets.all(15),
                      decoration: BoxDecoration(
                        color: Colors.black.withOpacity(0.2), // خلفية خفيفة جداً للنص
                        borderRadius: BorderRadius.circular(12),
                        border: Border.all(color: Colors.white.withOpacity(0.05)),
                      ),
                      child: _isLoading
                          ? const Center(
                              child: CircularProgressIndicator(strokeWidth: 2, color: Colors.blueAccent),
                            )
                          : _displayedResponse.isEmpty && !_isTyping
                              ? Center(
                                  child: Column(
                                    mainAxisSize: MainAxisSize.min,
                                    children: [
                                      Icon(Icons.auto_awesome, size: 40, color: Colors.grey.shade800),
                                      const SizedBox(height: 10),
                                      Text(
                                        _statusMessage,
                                        style: TextStyle(color: Colors.grey.shade700),
                                      ),
                                    ],
                                  ),
                                )
                              : SingleChildScrollView(
                                  controller: _scrollController,
                                  child: Directionality(
                                    textDirection: isRtl ? TextDirection.rtl : TextDirection.ltr,
                                    child: MarkdownBody(
                                      data: _displayedResponse,
                                      selectable: true,
                                      styleSheet: MarkdownStyleSheet(
                                        p: TextStyle(
                                          color: const Color(0xFFE0E0E0),
                                          fontSize: 15,
                                          height: 1.6,
                                          fontFamily: isRtl ? 'Sans' : 'Monospace',
                                        ),
                                        h1: TextStyle(color: Colors.blueAccent, fontSize: 22, fontWeight: FontWeight.bold, fontFamily: isRtl ? 'Sans' : 'Monospace'),
                                        h2: TextStyle(color: Colors.blueAccent, fontSize: 20, fontWeight: FontWeight.bold, fontFamily: isRtl ? 'Sans' : 'Monospace'),
                                        h3: const TextStyle(color: Colors.white, fontSize: 18, fontWeight: FontWeight.bold),
                                        code: const TextStyle(
                                          color: Color(0xFFff7b72),
                                          backgroundColor: Color(0xFF2d333b),
                                          fontFamily: 'Monospace',
                                          fontSize: 14,
                                        ),
                                        codeblockDecoration: BoxDecoration(
                                          color: const Color(0xFF22272e),
                                          borderRadius: BorderRadius.circular(8),
                                          border: Border.all(color: Colors.white10),
                                        ),
                                        blockquote: TextStyle(color: Colors.grey.shade400, fontStyle: FontStyle.italic),
                                        blockquoteDecoration: BoxDecoration(
                                          color: const Color(0xFF252525),
                                          borderRadius: BorderRadius.circular(4),
                                          border: Border(
                                            left: BorderSide(color: Colors.blueAccent, width: 4),
                                            right: BorderSide.none,
                                          ),
                                        ),
                                      ),
                                    ),
                                  ),
                                ),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}